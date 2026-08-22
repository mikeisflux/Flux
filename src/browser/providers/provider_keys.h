// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_
#define CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_

#include <string>

#include "chrome/browser/flux/secret_store.h"

class Profile;

namespace flux {

// Provider API keys, encrypted at rest.
//
// Just a SecretStore over the provider-key pref. It was its own copy of the
// encryptor plumbing until connectors needed the same thing for OAuth tokens;
// the mechanism is shared now and this names the one dictionary the LLM
// provider keys live in.
class ApiKeyStore : public SecretStore {
 public:
  explicit ApiKeyStore(Profile* profile);
  ~ApiKeyStore() override;
};

// Convenience wrappers that resolve the store from the profile's
// FluxAgentService. Return empty / no-op when the service or encryptor is not
// yet available.
std::string GetApiKey(Profile* profile, const std::string& provider);
void SetApiKey(Profile* profile,
               const std::string& provider,
               const std::string& key);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_
