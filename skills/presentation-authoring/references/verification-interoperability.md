# Verification and interoperability

Render the saved deck through an available office renderer, then inspect changed slides. Check title consistency, baseline alignment, chart axes and labels, overflow and speaker notes. Compare full-resolution dense slides and a whole-deck contact sheet; a successful serialization is not visual validation.

Round trips among PPTX, ODP, Keynote and web office applications can alter fonts, animation timing, transitions, embedded media and unsupported objects. Use the requested final application where available; retain the native source and a separate converted copy. Document features not tested. For headless LibreOffice use an isolated profile and explicit output directory; consult libreoffice-automation only when that process is needed.

Before editing an advanced existing deck with a general library, inventory media, charts, comments, relationships and animation parts. Prefer native editing for features the library cannot preserve. Do not regenerate every slide just to change a few words. Verify notes and hidden slides remain as requested; hidden content still exists in the deliverable.

PDF is a static preview, not a replacement for editable slides or a proof of working animation. For video export inspect duration, frame rate, aspect ratio and audio. Test slide builds and media playback in the presentation application when requested. If only static rendering is available, state that limitation. Give the user one clearly named final editable file and, if useful, a matching preview rather than ambiguous numbered intermediates.
