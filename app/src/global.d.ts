export {};

type EngineParams = {
  boostPercent?: number;
  eqGainsDb?: number[];
  bass?: number; clarity?: number; ambience?: number; surround?: number; dynamic?: number;
  outputGainDb?: number; limiterOn?: boolean; limiterCeilingDb?: number;
  enabled?: boolean;
};

export type LicenseStatus = {
  tier: 'free' | 'trial' | 'pro';
  maxBoostPercent: number;
  trialDaysLeft?: number;
  launch?: boolean;
  plan?: string;
  email?: string | null;
};

declare global {
  interface Window {
    api: {
      setParams: (p: EngineParams) => void;
      getStatus: () => Promise<{ installed: boolean; active: boolean; license: LicenseStatus }>;
      installEngine: () => Promise<{ installed: boolean; active: boolean }>;
      uninstallEngine: () => Promise<{ installed: boolean; active: boolean }>;
      activateLicense: (key: string) => Promise<{ ok: boolean; error?: string; plan?: string }>;
      deactivateLicense: () => Promise<LicenseStatus>;
      openBuy: () => void;
      toggleAutostart: (enable: boolean) => void;
      onEngineStatus: (cb: (d: { installed: boolean; active: boolean }) => void) => void;
      onLicenseStatus: (cb: (d: LicenseStatus) => void) => void;
      onHotkey: (cb: (d: 'up' | 'down') => void) => void;
    };
  }
}
