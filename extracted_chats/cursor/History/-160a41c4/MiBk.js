(function(){
  // Small runtime helper to block window.open calls (local debugging only)
  if (typeof window === 'undefined') return;
  if (window.__billing_popup_disabled) { console.log('billing popup blocker already installed'); return; }
  window.__billing_popup_disabled = true;
  const _origOpen = window.open;
  window.open = function(url, ...args) {
    try {
      // detect likely billing targets
      if (typeof url === 'string' && (url.includes('stripe') || url.includes('cursor.com') || url.includes('checkout') || url.includes('invoice') || url.includes('pay'))) {
        console.warn('[billing-block] blocked attempt to open:', url);
        console.trace();
        return null;
      }
    } catch (e) {
      console.warn('[billing-block] error while inspecting url', e);
    }
    return _origOpen.apply(this, [url, ...args]);
  };
  window.restoreBillingPopup = function() {
    window.open = _origOpen;
    delete window.__billing_popup_disabled;
    delete window.restoreBillingPopup;
    console.log('[billing-block] restored original window.open');
  };
  console.log('[billing-block] installed - call restoreBillingPopup() to undo');
})();
