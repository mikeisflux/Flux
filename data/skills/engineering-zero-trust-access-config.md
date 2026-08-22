---
name: Zero Trust Access Config
command: engineering-zero-trust-access-config
description: Design and review Cloudflare One Zero Trust/SASE: Access, Gateway, Tunnel, posture
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Cloudflare
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Configuring access to an internal application on the principle that the network is not a trust boundary.

## The model

No implicit trust from network location. Every request authenticates the user and the device, and is authorised against a policy, every time. In practice that means:

- **Identity** from a single provider, with MFA, and no local accounts on the application.
- **Device posture** as an input - managed, patched, disk-encrypted.
- **Per-application policy** rather than a flat VPN grant. Access to one service is not access to the estate.
- **Short-lived credentials.** A long-lived token is a network perimeter with extra steps.

## Configure in this order

1. Put the application behind the identity-aware proxy and confirm it is unreachable directly, from inside as well as outside.
2. Define the policy in terms of groups from the identity provider, never individual users.
3. Add device posture requirements as a second condition, once identity works.
4. Set session lifetime deliberately - shorter for higher-risk applications.
5. Turn on logging of allow *and* deny decisions before announcing it.

## Verify by trying to get in

Test as an unauthenticated user, as an authenticated user outside the group, from an unmanaged device, and with an expired session. A policy that has only been tested by someone who is allowed in has not been tested. Confirm the direct origin is genuinely blocked - a proxy in front of a still-public origin is theatre.

## Gotchas

- Service-to-service and CI traffic need their own identity, not an IP allowlist carved into the policy.
- Break-glass access must exist, be time-bound, and be loudly logged.
- Removing someone from a group must revoke live sessions, not just prevent new ones. Check that it does.
