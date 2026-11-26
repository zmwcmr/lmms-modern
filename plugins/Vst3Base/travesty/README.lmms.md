# Vendored Travesty headers

These headers are a copy of the pure C VST3-compatible interface named
"travesty" from the Carla project:

- upstream: https://github.com/DISTRHO/DPF (also shipped in Carla)
- Carla submodule commit: `66afe24a08790732cc17d81d4b846a1e0cfa0118`
- license: ISC, see the copyright header in each file
- original README: `README.txt`

They are kept as plain files so LMMS can build the VST3 scanner without
depending on the Carla submodule or on the Steinberg VST3 SDK. Do not edit
them; update the whole directory from upstream when needed.
