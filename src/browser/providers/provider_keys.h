// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_
#define CHROME_BROWSER_FLUX_PROVIDERS_PROVIDER_KEYS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace os_crypt_async {
class Encryptor;
}

namespace flux {

// Stores provider API keys encrypted at rest.
//
// The synchronous OSCrypt API no longer exists; encryption now goes through
// os_crypt_async, which vends an Encryptor asynchronously. The Encryptor
// itself has synchronous Encrypt/Decrypt, so this obtains one once at startup
// and holds it - callers stay synchronous.
//
// Keys are held in the profile's pref store and are deliberately NOT syncable:
// a key that follows a profile onto another machine is a key that leaks.
class ApiKeyStore {
 public:
  explicit ApiKeyStore(Profile* profile);
  ~ApiKeyStore();

  ApiKeyStore(const ApiKeyStore&) = delete;
  ApiKeyStore& operator=(const ApiKeyStore&) = delete;

  // False until the encryptor arrives. Reads return empty and writes are
  // refused before then rather than silently storing plaintext.
  bool ready() const { return encryptor_ != nullptr; }

  std::string Get(const std::string& provider) const;

  // Returns false if the key could not be encrypted. Nothing is stored in that
  // case - a plaintext fallback would defeat the point.
  bool Set(const std::string& provider, const std::string& key);

  void Clear(const std::string& provider);

 private:
  void OnEncryptorReady(scoped_refptr<os_crypt_async::Encryptor> encryptor);

  raw_ptr<Profile> profile_;
  scoped_refptr<os_crypt_async::Encryptor> encryptor_;
  base::WeakPtrFactory<ApiKeyStore> weak_factory_{this};
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
