// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

/**
 * Reads one of the console's packed JSON catalogues.
 *
 * XMLHttpRequest, NOT fetch, and this is not a style choice - fetch() cannot
 * load a chrome:// URL at all. Blink gates the whole API on a scheme registry
 * (`SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI`, checked
 * unconditionally in fetch_manager.cc), and the renderer registers only
 * chrome-untrusted:, devtools: and isolated-app: as fetch-capable -
 * `chrome:` is registered as WebUI, display-isolated and
 * not-allowing-javascript-URLs, and deliberately not as fetch-capable.
 * A fetch() therefore fails before it reaches the data source, with
 *
 *   Fetch API cannot load chrome://flux/skills.json.
 *   URL scheme "chrome" is not supported.
 *
 * XHR consults that same registry in exactly one place - deciding whether a
 * request MAY CARRY A BODY (`XMLHttpRequest::AreMethodAndURLValidForSend`) -
 * so a same-origin GET is unaffected and reaches the WebUIDataSource
 * normally. This is how Chromium's own WebUIs have always read their packed
 * resources.
 *
 * The alternative is patching render_thread_impl.cc to make chrome:
 * fetch-capable, which would change the security posture of every WebUI in
 * the browser to save three call sites here. Not worth it.
 */
export function loadPackedJson<T>(path: string): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const request = new XMLHttpRequest();
    request.open('GET', path, true);
    request.responseType = 'json';
    request.onload = () => {
      // A WebUIDataSource answers 404 for a path it does not know, and
      // responseType='json' leaves `response` null on a body it cannot parse.
      // Both are bugs in the build rather than runtime conditions, so they say
      // which file rather than failing as a generic type error later.
      if (request.status !== 200 && request.status !== 0) {
        reject(new Error(`${path} returned HTTP ${request.status}.`));
        return;
      }
      if (request.response === null) {
        reject(new Error(`${path} is missing or is not valid JSON.`));
        return;
      }
      resolve(request.response as T);
    };
    request.onerror = () => reject(new Error(`${path} could not be read.`));
    request.send();
  });
}

/**
 * A once-only loader that does not cache a failure. A rejected promise left in
 * a module-level cache is permanent for the life of the page: every later
 * caller gets the original error and nothing ever retries, which turns one
 * transient miss into a screen that is broken until the tab is closed.
 */
export function once<T>(load: () => Promise<T>): () => Promise<T> {
  let cached: Promise<T>|null = null;
  return () => {
    if (!cached) {
      cached = load().catch(error => {
        cached = null;
        throw error;
      });
    }
    return cached;
  };
}
