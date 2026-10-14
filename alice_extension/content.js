// Alice Bridge — content script for chatgpt.com
console.log('[Alice Bridge] content.js loaded on', location.hostname, location.pathname);

let responseCheckInterval = null;

// ── Find the ChatGPT input box ────────────────────────────────────────────────
function getInputBox() {
  return (
    document.querySelector('#prompt-textarea') ||
    document.querySelector('div[contenteditable="true"]')
  );
}

// ── Find the last assistant message ──────────────────────────────────────────
function getLastAssistantText() {
  const msgs = document.querySelectorAll('div[data-message-author-role="assistant"]');
  if (!msgs.length) return '';
  return msgs[msgs.length - 1].innerText.trim();
}

// ── Inject prompt and submit ──────────────────────────────────────────────────
function sendPrompt(prompt) {
  console.log('[Alice Bridge] sendPrompt (' + prompt.length + ' chars)');

  const box = getInputBox();
  if (!box) {
    console.error('[Alice Bridge] Input box not found — page may still be loading');
    return;
  }

  box.focus();
  box.click();

  if (box.tagName === 'TEXTAREA') {
    const nativeSetter = Object.getOwnPropertyDescriptor(
      window.HTMLTextAreaElement.prototype, 'value'
    ).set;
    nativeSetter.call(box, prompt);
  } else {
    box.innerHTML = '';
    box.innerText  = prompt;
  }

  box.dispatchEvent(new InputEvent('input', { bubbles: true, composed: true }));

  setTimeout(() => {
    box.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Enter', code: 'Enter', keyCode: 13,
      bubbles: true, composed: true,
    }));
    watchForResponse();
  }, 400);
}

// ── Poll until a NEW response (past the baseline) is stable ──────────────────
function watchForResponse() {
  if (responseCheckInterval) {
    clearInterval(responseCheckInterval);
    responseCheckInterval = null;
  }

  // Snapshot whatever is already on screen so we don't mistake the previous
  // assistant message for the new response.
  const baseline    = getLastAssistantText();
  let previousText  = baseline;
  let stableCount   = 0;
  const STABLE_NEEDED = 4;    // 4 × 500 ms = 2 s stable → done
  let pollCount     = 0;
  const MAX_POLLS   = 240;    // 2-minute timeout

  console.log('[Alice Bridge] watchForResponse — baseline length:', baseline.length);

  // Give ChatGPT a moment to start generating before we begin polling.
  setTimeout(() => {
    responseCheckInterval = setInterval(() => {
      pollCount++;
      const current = getLastAssistantText();

      if (pollCount % 10 === 0) {
        console.log('[Alice Bridge] Poll #' + pollCount +
                    ' len:' + current.length + ' stable:' + stableCount);
      }

      if (pollCount >= MAX_POLLS) {
        console.error('[Alice Bridge] Timeout — no stable response after 2 min');
        clearInterval(responseCheckInterval);
        responseCheckInterval = null;
        return;
      }

      // Ignore text that hasn't moved past the baseline yet.
      if (!current || current === baseline) {
        stableCount  = 0;
        previousText = current;
        return;
      }

      if (current === previousText) {
        stableCount++;
      } else {
        stableCount  = 0;
        previousText = current;
      }

      if (stableCount >= STABLE_NEEDED) {
        console.log('[Alice Bridge] Response stable (' + current.length + ' chars) — sending');
        clearInterval(responseCheckInterval);
        responseCheckInterval = null;

        chrome.runtime.sendMessage({ action: 'responseReady', text: current }, (resp) => {
          if (chrome.runtime.lastError) {
            console.error('[Alice Bridge] sendMessage to background failed:',
                          chrome.runtime.lastError.message);
          } else {
            console.log('[Alice Bridge] Background acknowledged:', resp);
          }
        });
      }
    }, 500);
  }, 3000);
}

// ── Listen for messages from background.js ───────────────────────────────────
chrome.runtime.onMessage.addListener((msg, _sender, sendResponse) => {
  if (msg.action === 'sendPrompt' && msg.prompt) {
    if (msg.prompt === 'NEW_CHAT') {
      console.log('[Alice Bridge] NEW_CHAT — navigating to chatgpt.com/');
      sendResponse({ status: 'ok' });
      window.location.href = 'https://chatgpt.com/';
    } else {
      sendPrompt(msg.prompt);
      sendResponse({ status: 'ok' });
    }
  }
  return true;
});
