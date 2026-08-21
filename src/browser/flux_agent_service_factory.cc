// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/flux_agent_service_factory.h"

#include "chrome/browser/flux/flux_agent_service.h"
#include "chrome/browser/profiles/profile.h"

namespace flux {

// static
FluxAgentService* FluxAgentServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FluxAgentService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
FluxAgentServiceFactory* FluxAgentServiceFactory::GetInstance() {
  static base::NoDestructor<FluxAgentServiceFactory> instance;
  return instance.get();
}

FluxAgentServiceFactory::FluxAgentServiceFactory()
    : ProfileKeyedServiceFactory(
          "FluxAgentService",
          // Each profile gets its own service and its own runs. This is what
          // keeps concurrent tasks in different profiles from sharing cookies
          // or a logged-in identity.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // No agent in Incognito: runs persist history and write to a
              // scheduler, neither of which belongs in an off-the-record
              // profile.
              .WithGuest(ProfileSelection::kNone)
              .Build()) {}

FluxAgentServiceFactory::~FluxAgentServiceFactory() = default;

std::unique_ptr<KeyedService>
FluxAgentServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FluxAgentService>(
      Profile::FromBrowserContext(context));
}

bool FluxAgentServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Created eagerly so the workflow scheduler is running before the user
  // opens the console. A scheduled 7am task must fire whether or not
  // chrome://flux has ever been visited this session.
  return true;
}

}  // namespace flux
