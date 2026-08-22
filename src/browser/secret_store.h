// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_SECRET_STORE_H_
#define CHROME_BROWSER_FLUX_SECRET_STORE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace os_crypt_async {
class Encryptor;
}

namespace flux {

// Named secrets, encrypted at rest, in one dictionary pref.
//
// The synchronous OSCrypt API no longer exists; encryption goes through
// os_crypt_async, which vends an Encryptor asynchronously. The Encryptor
// itself has synchronous Encrypt/Decrypt, so this obtains one once at startup
// and holds it - callers stay synchronous.
//
// ONE dictionary pref holding many named secrets, rather than a pref per
// secret. That is not a tidiness preference: PrefService::GetString on an
// unregistered path is a hard CHECK - "Trying to access an unregistered pref"
// - which takes the browser process down, and writes to one hit
// DUMP_WILL_BE_NOTREACHED and are silently dropped. A pref whose name embeds a
// provider or connector id cannot be registered up front, so the name has to
// be a key inside a registered dictionary instead.
//
// Never syncable. A secret that follows a profile onto another machine is a
// secret that leaks.
class SecretStore {
 public:
  SecretStore(Profile* profile, std::string pref_name);
  virtual ~SecretStore();

  SecretStore(const SecretStore&) = delete;
  SecretStore& operator=(const SecretStore&) = delete;

  // False until the encryptor arrives. Reads return empty and writes are
  // refused before then rather than silently storing plaintext.
  bool ready() const { return encryptor_ != nullptr; }

  std::string Get(const std::string& name) const;

  // Returns false if the value could not be encrypted. Nothing is stored in
  // that case - a plaintext fallback would defeat the point.
  bool Set(const std::string& name, const std::string& value);

  void Clear(const std::string& name);

  // Names that currently hold a value. Does not decrypt, so it is safe to call
  // before the encryptor arrives - which is what the connectors screen needs
  // in order to render "connected" without handling a token.
  std::vector<std::string> Names() const;

  bool Has(const std::string& name) const;

 private:
  void OnEncryptorReady(scoped_refptr<os_crypt_async::Encryptor> encryptor);

  raw_ptr<Profile> profile_;
  const std::string pref_name_;
  scoped_refptr<os_crypt_async::Encryptor> encryptor_;
  base::WeakPtrFactory<SecretStore> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_SECRET_STORE_H_
