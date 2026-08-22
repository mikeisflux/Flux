// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.
//
// A stand-in for the bindings mojom generates during the build, so the console
// can be type-checked in this container, in seconds, without a Chromium
// checkout. A `.ts` error is otherwise a build failure ninety minutes in.
//
// KEEP IN SYNC WITH src/browser/mojom/flux.mojom. It is transcribed by hand
// rather than generated, so the failure mode is a missing member here rather
// than a wrong one: tsc reports anything the console calls that this file does
// not declare, which is the signal to update it.
//
// Naming follows mojom's TS convention: snake_case fields become lowerCamel,
// enum values keep their k-prefix, and every method returns a promise of a
// struct named after its reply parameters.

export declare enum RunState {
  kQueued,
  kRunning,
  kAwaitingApproval,
  kPaused,
  kSucceeded,
  kFailed,
  kCancelled,
}

export declare enum Provider {
  kAnthropic,
  kOpenAI,
}

export declare enum WriteScope {
  kReadOnly,
  kDraft,
  kSend,
  kPurchase,
}

export interface Url {
  url: string;
}

export interface Time {
  internalValue: bigint;
}

export interface ActionRecord {
  toolName: string;
  summary: string;
  pageUrl: Url|null;
  startedAt: Time;
  finishedAt: Time;
  succeeded: boolean;
  error: string|null;
  wasApproved: boolean;
}

export interface ApprovalRequest {
  runId: string;
  toolName: string;
  rationale: string;
  effectSummary: string;
  pageUrl: Url|null;
  payloadPreview: string|null;
}

export interface RunProgress {
  runId: string;
  state: RunState;
  currentStep: string|null;
  actionsTaken: number;
  inputTokens: number;
  outputTokens: number;
  creditsSpent: bigint;
}

export interface ModelConfig {
  provider: Provider;
  model: string;
  maxOutputTokens: number;
  allowFailover: boolean;
}

export interface TaskSpec {
  prompt: string;
  templateId: string|null;
  writeScope: WriteScope;
  model: ModelConfig;
  profileId: string;
  creditBudget: bigint;
}

export interface LearnedFact {
  id: string;
  text: string;
  sourceRunId: string;
  learnedAt: Time;
}

export interface ConnectorStatus {
  id: string;
  connectable: boolean;
  hasClient: boolean;
  connected: boolean;
  expired: boolean;
  detail: string|null;
}

export interface ProviderKeyStatus {
  provider: Provider;
  configured: boolean;
  hint: string;
  validated: boolean;
  lastError: string|null;
}

export declare class FluxPageHandlerRemote {
  $: {
    bindNewPipeAndPassReceiver(): unknown,
  };

  startRun(spec: TaskSpec): Promise<{runId: string, error: string|null}>;
  cancelRun(runId: string): void;
  pauseRun(runId: string): void;
  resumeRun(runId: string): void;
  resolveApproval(runId: string, approved: boolean, userNote: string|null):
      void;
  listRuns(): Promise<{runs: RunProgress[]}>;
  getActions(runId: string): Promise<{actions: ActionRecord[]}>;
  compileReplay(runId: string):
      Promise<{workflowId: string|null, error: string|null}>;
  getConcurrencyLimit():
      Promise<{limit: number, active: number, queued: number}>;
  listProviderKeys(): Promise<{statuses: ProviderKeyStatus[]}>;
  setProviderKey(provider: Provider, key: string):
      Promise<{stored: boolean, error: string|null}>;
  clearProviderKey(provider: Provider): void;
  validateProviderKey(provider: Provider):
      Promise<{valid: boolean, error: string|null}>;
  getInstructions(): Promise<{text: string, learned: LearnedFact[]}>;
  setInstructions(text: string): void;
  dismissLearnedFact(id: string): void;
  listAdoptedSkills(): Promise<{commands: string[]}>;
  adoptSkill(command: string, name: string, description: string,
             instructions: string):
      Promise<{adopted: boolean, error: string|null}>;
  removeSkill(command: string): void;
  listConnectors(): Promise<{statuses: ConnectorStatus[]}>;
  setConnectorClient(connectorId: string, clientId: string,
                     clientSecret: string, redirectUri: string):
      Promise<{stored: boolean, error: string|null}>;
  getConnectorClient(connectorId: string):
      Promise<{clientId: string, redirectUri: string, hasSecret: boolean}>;
  beginConnect(connectorId: string):
      Promise<{started: boolean, error: string|null}>;
  setPersonalToken(connectorId: string, token: string):
      Promise<{stored: boolean, error: string|null}>;
  disconnect(connectorId: string): void;
  getSidebarCollapsed(): Promise<{collapsed: boolean}>;
  setSidebarCollapsed(collapsed: boolean): void;
}

// Every method the observer must implement, so that a class handed to the
// receiver below is checked against the whole interface.
//
// Typing the receiver's constructor as `object` instead was a real build
// break: adding OnConnectorChanged to the mojom left FluxSidebar not
// implementing it, tsc here was happy, and the failure surfaced 28 targets
// into a Chromium build. mojom generates exactly this interface - keep every
// FluxPageHandlerObserver method in it.
export interface FluxPageHandlerObserverInterface {
  onRunProgress(progress: RunProgress): void;
  onAction(runId: string, action: ActionRecord): void;
  onApprovalRequested(request: ApprovalRequest): void;
  onRunFinished(runId: string, finalState: RunState,
                summary: string|null): void;
  onLearnedFact(fact: string, sourceRunId: string): void;
  onConnectorChanged(status: ConnectorStatus, error: string|null): void;
}

export declare class FluxPageHandlerObserverReceiver {
  constructor(impl: FluxPageHandlerObserverInterface);
  $: {
    bindNewPipeAndPassRemote(): unknown,
  };
}

export declare const FluxPageHandlerFactory: {
  getRemote(): {
    createPageHandler(observer: unknown, handler: unknown): void,
  },
};
