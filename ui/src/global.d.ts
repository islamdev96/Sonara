export {};

declare global {
  interface Window {
    api: {
      setMasterBoost: (boostPercent: number) => void;
      setEqBands: (bandsArray: number[]) => void;
      toggleAutoStart: (enable: boolean) => void;
      installAPO: () => void;
      onBoostStatus: (callback: (data: any) => void) => void;
      onHotkeyAction: (callback: (action: string) => void) => void;
      onAPOStatus: (callback: (data: { installed: boolean }) => void) => void;
      onVolumeSync: (callback: (level: number) => void) => void;
    };
  }
}
