# Analyzer Validation Tests

The analyzer validation signal is analyzer-only. It must not be audible at the
plugin output and must stay disabled by default.

## Mono / In Phase

Mode: Validation Mono Sine or Stereo Sine In Phase

Expected:
- Broadband correlation near +1
- Goniometer vertical
- L/R Dual curves identical
- M/S Dual: Mid visible, Side silent
- 31-band correlation active band near +1 around 1 kHz

## Out Of Phase

Mode: Stereo Sine Out Of Phase

Expected:
- Broadband correlation near -1
- Goniometer horizontal
- L/R Dual curves identical level, opposite phase not visible in spectrum
- M/S Dual: Mid silent, Side visible
- 31-band correlation active band near -1 around 1 kHz

## Left Only

Expected:
- Balance left
- L/R Dual: L visible, R silent
- M/S Dual: Mid and Side similar level
- Broadband correlation may be invalid/near 0
- Frequency correlation should not fake +1

## Right Only

Expected:
- Balance right
- L/R Dual: R visible, L silent
- M/S Dual: Mid visible, Side inverted but same spectrum level
- Broadband correlation may be invalid/near 0

## Music-like Stereo

Expected:
- Broadband correlation between -1 and +1
- 31-band correlation varies across bands
- Loudness values update
