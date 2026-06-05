export {};

declare global {
  interface Window {
    api: {
      setMasterBoost: (boostPercent: number) => void;
      setEqBands: (bandsArray: number[]) => void;
      onBoostStatus: (callback: (response: any) => void) => void;
      onHotkeyAction: (callback: (action: string) => void) => void;
    };
  }
}
