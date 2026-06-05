export {};

declare global {
  interface Window {
    api: {
      setMasterBoost: (boostPercent: number) => void;
      onBoostStatus: (callback: (response: any) => void) => void;
    };
  }
}
