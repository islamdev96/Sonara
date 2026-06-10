// Bilingual UI strings (English / العربية). `Strings` is the shape components
// receive as the `t` prop.
export type Lang = 'en' | 'ar'

export const i18n = {
  en: {
    title: 'Sonara', settings: 'SETTINGS', lang: 'عربي', autoStart: 'Run on Startup',
    presets: 'PRESETS', saveNew: 'Save Preset', overwrite: 'Update Preset', deletePreset: 'Delete Preset',
    device: 'System Default Output', master: 'Boost', clarity: 'Clarity', ambience: 'Ambience',
    surround: 'Surround', dynamic: 'Loudness', bass: 'Bass', limiter: 'Safe Limiter',
    engineActive: 'Engine: Active — built-in, no extra software', engineMissing: 'Engine: Not installed — boost limited to 100%',
    install: 'Install Engine', enterName: 'Preset name', save: 'Save', cancel: 'Cancel', confirmDel: 'Delete this preset?', delete: 'Delete',
    trial: (d: number) => `Pro trial — ${d} day${d === 1 ? '' : 's'} left`, free: 'Free — upgrade for boost above 100%', pro: 'Pro — activated', launchFree: 'Free — full version (launch offer)',
    buy: 'Upgrade', activate: 'Activate', licenseKey: 'Paste license key', activated: 'Activated!', badKey: 'Invalid key',
  },
  ar: {
    title: 'سونارا', settings: 'الإعدادات', lang: 'English', autoStart: 'التشغيل مع الويندوز',
    presets: 'الإعدادات المسبقة', saveNew: 'حفظ إعداد', overwrite: 'تحديث الإعداد', deletePreset: 'حذف الإعداد',
    device: 'جهاز الإخراج الافتراضي', master: 'التضخيم', clarity: 'الوضوح', ambience: 'المحيط',
    surround: 'الإحاطة', dynamic: 'الجهارة', bass: 'الجهير', limiter: 'الحد الآمن',
    engineActive: 'المحرك: مفعّل — مدمج بدون برامج إضافية', engineMissing: 'المحرك: غير مثبّت — التضخيم محدود بـ 100%',
    install: 'تثبيت المحرك', enterName: 'اسم الإعداد', save: 'حفظ', cancel: 'إلغاء', confirmDel: 'حذف هذا الإعداد؟', delete: 'حذف',
    trial: (d: number) => `نسخة Pro تجريبية — باقٍ ${d} يوم`, free: 'مجاني — قم بالترقية للتضخيم فوق 100%', pro: 'Pro — مُفعّل', launchFree: 'مجاني بالكامل — عرض الإطلاق',
    buy: 'ترقية', activate: 'تفعيل', licenseKey: 'الصق مفتاح الترخيص', activated: 'تم التفعيل!', badKey: 'مفتاح غير صالح',
  },
}

export type Strings = typeof i18n.en
