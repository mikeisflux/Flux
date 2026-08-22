// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/providers/provider_keys.h"

#include "chrome/browser/flux/flux_agent_service.h"
#include "chrome/browser/flux/flux_agent_service_factory.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"

namespace flux {

ApiKeyStore::ApiKeyStore(Profile* profile)
    : SecretStore(profile, prefs::kApiKeys) {}

ApiKeyStore::~ApiKeyStore() = default;

std::string GetApiKey(Profile* profile, const std::string& provider) {
  if (!profile)
    return std::string();
  FluxAgentService* service = FluxAgentServiceFactory::GetForProfile(profile);
  return service && service->keys() ? service->keys()->Get(provider)
                                    : std::string();
}

void SetApiKey(Profile* profile,
               const std::string& provider,
               const std::string& key) {
  if (!profile)
    return;
  FluxAgentService* service = FluxAgentServiceFactory::GetForProfile(profile);
  if (service && service->keys())
    service->keys()->Set(provider, key);
}

}  // namespace flux
