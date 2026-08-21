// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/webui/flux_page_handler.h"

#include <utility>

#include "chrome/browser/flux/flux_agent_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace flux {

FluxPageHandler::FluxPageHandler(
    mojo::PendingReceiver<mojom::FluxPageHandler> receiver,
    mojo::PendingRemote<mojom::FluxPageHandlerObserver> observer,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      service_(FluxAgentServiceFactory::GetForProfile(profile)),
      receiver_(this, std::move(receiver)),
      observer_(std::move(observer)) {
  if (service_)
    observation_.Observe(service_.get());
}

FluxPageHandler::~FluxPageHandler() = default;

void FluxPageHandler::StartRun(mojom::TaskSpecPtr spec,
                               StartRunCallback callback) {
  if (!service_) {
    std::move(callback).Run(std::string(), "Agent service unavailable.");
    return;
  }
  std::string error;
  std::optional<std::string> run_id =
      service_->StartRun(std::move(spec), &error);
  if (!run_id) {
    std::move(callback).Run(std::string(), error);
    return;
  }
  std::move(callback).Run(*run_id, std::nullopt);
}

void FluxPageHandler::CancelRun(const std::string& run_id) {
  if (service_)
    service_->CancelRun(run_id);
}

void FluxPageHandler::PauseRun(const std::string& run_id) {
  if (service_)
    service_->PauseRun(run_id);
}

void FluxPageHandler::ResumeRun(const std::string& run_id) {
  if (service_)
    service_->ResumeRun(run_id);
}

void FluxPageHandler::ResolveApproval(
    const std::string& run_id,
    bool approved,
    const std::optional<std::string>& user_note) {
  if (service_)
    service_->ResolveApproval(run_id, approved, user_note.value_or(""));
}

void FluxPageHandler::ListRuns(ListRunsCallback callback) {
  std::move(callback).Run(service_ ? service_->ListRuns()
                                   : std::vector<mojom::RunProgressPtr>());
}

void FluxPageHandler::GetActions(const std::string& run_id,
                                 GetActionsCallback callback) {
  std::move(callback).Run(service_ ? service_->GetActions(run_id)
                                   : std::vector<mojom::ActionRecordPtr>());
}

void FluxPageHandler::CompileReplay(const std::string& run_id,
                                    CompileReplayCallback callback) {
  if (!service_) {
    std::move(callback).Run(std::nullopt, "Agent service unavailable.");
    return;
  }
  std::string error;
  std::optional<std::string> workflow_id =
      service_->CompileReplay(run_id, &error);
  std::move(callback).Run(workflow_id,
                          workflow_id ? std::nullopt
                                      : std::make_optional(error));
}

void FluxPageHandler::GetConcurrencyLimit(
    GetConcurrencyLimitCallback callback) {
  if (!service_) {
    std::move(callback).Run(0, 0, 0);
    return;
  }
  // Surfacing active and queued separately is what lets the console say
  // "4 running, 2 queued" rather than leaving a queued task looking stalled.
  FluxAgentService::Concurrency c = service_->GetConcurrency();
  std::move(callback).Run(c.limit, c.active, c.queued);
}

void FluxPageHandler::OnRunProgress(const mojom::RunProgress& progress) {
  observer_->OnRunProgress(progress.Clone());
}

void FluxPageHandler::OnRunAction(const std::string& run_id,
                                  const mojom::ActionRecord& action) {
  observer_->OnAction(run_id, action.Clone());
}

void FluxPageHandler::OnApprovalRequested(
    const mojom::ApprovalRequest& request) {
  observer_->OnApprovalRequested(request.Clone());
}

void FluxPageHandler::OnRunFinished(const std::string& run_id,
                                    mojom::RunState state,
                                    const std::string& summary) {
  observer_->OnRunFinished(run_id, state, summary);
}

}  // namespace flux
