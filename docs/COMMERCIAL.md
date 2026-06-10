# Commercial plan

## Positioning

Sonara is a **self-contained** Windows volume booster and audio
enhancer. Unlike tools that are just a UI on top of the open-source Equalizer
APO, we ship our **own audio engine**, so there is nothing else to install and we
control the entire sound quality + safety story.

### Competitive landscape

| Product | Boost | EQ | Own engine | Notes |
|---------|:-----:|:--:|:----------:|-------|
| **Sonara** | ✅ +500% safe | ✅ 10-band | ✅ | Look-ahead limiter, bilingual, one install |
| FxSound (ex-DFX) | ✅ | ✅ | ✅ | Established; open-core |
| Letasoft Sound Booster | ✅ | ❌ | ✅ | Boost only, dated UI |
| Boom 3D | ✅ | ✅ | ✅ | Strong effects, pricier |
| Equalizer APO + Peace | ✅ | ✅ | — | Free, technical, no support |

### Our edges
- Safety: brick-wall limiter means big boosts without clipping/damage.
- Simplicity: single installer, no driver hunting.
- Arabic-first bilingual UX (underserved by competitors).
- Clean modular DSP core reusable for future products (mic, macOS).

## Launch strategy: free first

**Phase 1 (launch — now):** the full app is **100% free**, all features unlocked, to grow the user base and reviews as fast as possible. In code this is the `LAUNCH_FREE` flag in `app/electron/licensing.cjs`.

**Phase 2 (monetize later):** flip `LAUNCH_FREE` to `false` to activate the tiers below (existing free users can be grandfathered or offered a discount).

## Pricing (Phase 2 — proposed)

| Tier | Price | Features |
|------|-------|----------|
| **Free** | €0 | Boost up to 100%, 10-band EQ, basic presets. |
| **Pro (lifetime)** | €24.99 one-time | Boost to +500%, all enhancers, custom presets, updates 1 yr. |
| **Pro (subscription)** | €2.49/mo or €17.99/yr | Same as lifetime + continuous updates + priority support. |
| **Trial** | 14 days | All Pro features, then drops to Free. |

## Licensing technology

- Keys are **Ed25519-signed** offline tokens: `base64url(payload).base64url(sig)`.
- The app ships only the **public** key, so keys cannot be forged.
- Payload carries `{ email, plan, exp? }`; subscriptions embed an expiry.
- Optional future online activation endpoint (pluggable in `licensing.cjs`).
- Trial state stored in `%ProgramData%\WinAudioBoosterPro\.trial`.

## Go-to-market

1. Landing site `winaudioboosterpro.com` with download + buy (Paddle/Lemon Squeezy
   handle VAT + key delivery).
2. SEO around "volume booster Windows", "مضخم صوت للكمبيوتر", "FxSound alternative".
3. Microsoft Store listing (packaged MSIX) as a second channel.
4. Content + comparison reviews; affiliate/influencer for gaming & audio niches.

## Pre-launch requirements

- [ ] **EV code-signing certificate** (mandatory: Windows won't load an unsigned APO).
- [ ] QA matrix: Windows 10 & 11, shared vs. exclusive mode, USB DACs, Bluetooth, HDMI.
- [ ] Privacy policy + EULA + refund policy.
- [ ] Payment + license-delivery integration.
- [ ] Support channel (email/helpdesk) and docs site.
