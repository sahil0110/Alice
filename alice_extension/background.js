// Alice Bridge — background service worker

const HOST_NAME       = 'com.alice.bridge';
const RECONNECT_DELAY = 3000; // ms before reconnect attempt

let nativePort   = null;
let chatGptTabId = null;

// ── Keep service worker alive via alarms (MV3 workers can be killed by Chrome) ─
chrome.alarms.create('keepAlive', { periodInMinutes: 0.4 }); // ~24 s
chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === 'keepAlive') {
    console.log('[Alice Bridge] keepAlive tick — port:', nativePort ? 'open' : 'null');
    if (!nativePort) {
      console.log('[Alice Bridge] Port gone — reconnecting...');
      connectNative();
    }
  }
});

// ── Native messaging ──────────────────────────────────────────────────────────
function connectNative() {
  console.log('[Alice Bridge] Attempting connectNative:', HOST_NAME);
  try {
    nativePort = chrome.runtime.connectNative(HOST_NAME);
    console.log('[Alice Bridge] Native port opened');

    nativePort.onMessage.addListener((msg) => {
      console.log('[Alice Bridge] ← Native host:', JSON.stringify(msg));
      if (msg && msg.prompt) {
        console.log('[Alice Bridge] Forwarding prompt (' + msg.prompt.length + ' chars)');
        forwardPromptToTab(msg.prompt);
      } else {
        console.warn('[Alice Bridge] Native message has no prompt field:', msg);
      }
    });

    nativePort.onDisconnect.addListener(() => {
      const err = chrome.runtime.lastError;
      console.warn('[Alice Bridge] Native host disconnected:', err?.message ?? '(no error)');
      nativePort = null;
      setTimeout(connectNative, RECONNECT_DELAY);
    });

  } catch (e) {
    console.error('[Alice Bridge] connectNative threw:', e);
    nativePort = null;
    setTimeout(connectNative, RECONNECT_DELAY);
  }
}

// ── Send prompt to a tab, injecting content.js first if it isn't loaded ───────
// onFail() is called if both the initial send AND the post-injection retry fail.
function sendToTab(tabId, prompt, onFail) {
  chrome.tabs.sendMessage(tabId, { action: 'sendPrompt', prompt }, (resp) => {
    if (!chrome.runtime.lastError) {
      console.log('[Alice Bridge] Tab', tabId, 'acknowledged:', resp);
      return;
    }

    // "Receiving end does not exist" — content script not loaded in this tab.
    // Inject it now and retry.
    const err = chrome.runtime.lastError.message;
    console.warn('[Alice Bridge] sendMessage to tab', tabId, 'failed:', err,
                 '— injecting content.js');

    chrome.scripting.executeScript(
      { target: { tabId }, files: ['content.js'] },
      () => {
        if (chrome.runtime.lastError) {
          console.error('[Alice Bridge] Script injection failed:',
                        chrome.runtime.lastError.message);
          if (onFail) onFail();
          return;
        }
        console.log('[Alice Bridge] content.js injected — retrying sendMessage in 700ms');
        setTimeout(() => {
          chrome.tabs.sendMessage(tabId, { action: 'sendPrompt', prompt }, (resp2) => {
            if (chrome.runtime.lastError) {
              console.error('[Alice Bridge] Retry also failed:',
                            chrome.runtime.lastError.message);
              if (onFail) onFail();
            } else {
              console.log('[Alice Bridge] Retry acknowledged:', resp2);
            }
          });
        }, 700);
      }
    );
  });
}

// ── Forward prompt to the ChatGPT tab ─────────────────────────────────────────
function forwardPromptToTab(prompt) {
  console.log('[Alice Bridge] forwardPromptToTab — cached tabId:', chatGptTabId);

  if (chatGptTabId !== null) {
    // Verify the cached tab still exists before using it
    chrome.tabs.get(chatGptTabId, (tab) => {
      if (chrome.runtime.lastError || !tab) {
        console.warn('[Alice Bridge] Cached tab gone:', chrome.runtime.lastError?.message);
        chatGptTabId = null;
        findAndForward(prompt);
        return;
      }
      console.log('[Alice Bridge] Sending to cached tab', chatGptTabId, tab.url);
      sendToTab(chatGptTabId, prompt, () => {
        // Both attempts failed — clear cached ID and try fresh
        chatGptTabId = null;
        findAndForward(prompt);
      });
    });
  } else {
    findAndForward(prompt);
  }
}

function findAndForward(prompt) {
  console.log('[Alice Bridge] Querying for chatgpt.com tabs...');
  chrome.tabs.query({ url: 'https://chatgpt.com/*' }, (tabs) => {
    if (chrome.runtime.lastError) {
      console.error('[Alice Bridge] tabs.query error:', chrome.runtime.lastError.message);
      return;
    }
    console.log('[Alice Bridge] Found', tabs.length, 'chatgpt tab(s)');

    if (!tabs.length) {
      // No ChatGPT tab — open one and wait for it to finish loading
      console.log('[Alice Bridge] No ChatGPT tab — opening one');
      chrome.tabs.create({ url: 'https://chatgpt.com/' }, (tab) => {
        chatGptTabId = tab.id;
        console.log('[Alice Bridge] Created tab', tab.id, '— waiting for load');
        chrome.tabs.onUpdated.addListener(function listener(tabId, info) {
          if (tabId === tab.id && info.status === 'complete') {
            chrome.tabs.onUpdated.removeListener(listener);
            console.log('[Alice Bridge] New tab loaded — sending prompt after 2500ms');
            setTimeout(() => {
              console.log('[Alice Bridge] Sending to new tab', chatGptTabId);
              sendToTab(chatGptTabId, prompt, () => {
                console.error('[Alice Bridge] Failed to send to newly created tab');
              });
            }, 2500);
          }
        });
      });

    } else {
      // Use the first existing ChatGPT tab
      chatGptTabId = tabs[0].id;
      console.log('[Alice Bridge] Using existing tab', chatGptTabId, tabs[0].url);
      sendToTab(chatGptTabId, prompt, () => {
        // Content script couldn't be loaded even after injection — give up on this tab
        console.error('[Alice Bridge] Giving up on tab', chatGptTabId);
        chatGptTabId = null;
      });
    }
  });
}

// ── Listen for messages from content.js ──────────────────────────────────────
chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  console.log('[Alice Bridge] ← content.js:', msg?.action, 'from tab', sender.tab?.id);

  if (sender.tab) {
    chatGptTabId = sender.tab.id;
  }

  if (msg.action === 'responseReady') {
    const preview = msg.text?.slice(0, 100) ?? '';
    console.log('[Alice Bridge] Response ready (' + (msg.text?.length ?? 0) + ' chars):', preview);

    if (nativePort) {
      console.log('[Alice Bridge] → Forwarding response to native host');
      nativePort.postMessage({ response: msg.text });
    } else {
      console.error('[Alice Bridge] No native port — response DROPPED');
    }
    sendResponse({ status: 'ok' });
  }

  return true;
});

// ── Track tab removal ─────────────────────────────────────────────────────────
chrome.tabs.onRemoved.addListener((tabId) => {
  if (tabId === chatGptTabId) {
    console.log('[Alice Bridge] ChatGPT tab closed:', tabId);
    chatGptTabId = null;
  }
});

// ── Start ─────────────────────────────────────────────────────────────────────
console.log('[Alice Bridge] background.js starting up');
connectNative();
