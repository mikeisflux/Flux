// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_
#define CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_

#include <string>

class Profile;

namespace flux {

// Returns the stored API key for `provider` ("anthropic" | "openai"), or empty.
//
// Keys are held in the profile's pref store encrypted with OSCrypt - the same
// mechanism Chromium uses for saved passwords, which means DPAPI on Windows and
// the Keychain on macOS. They are deliberately NOT synced: a key that follows a
// profile onto another machine is a key that leaks.
std::string GetApiKey(Profile* profile, const std::string& provider);

void SetApiKey(Profile* profile,
               const std::string& provider,
               const std::string& key);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_
