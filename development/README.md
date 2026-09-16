# Development prerequisites

After cloning on a new Windows PC, run:

```powershell
.\development\Setup-Development.ps1
```

It verifies Qt 6.8.3 Desktop `win64_msvc2022_64` at
`development\Qt\6.8.3\msvc2022_64`. The SDK is versioned with this repository,
so a complete clone does not need or use `C:\Qt`.

The machine still needs Visual Studio 2026 with the MSVC C++ workload and Windows SDK.
After the requirement passes:

```powershell
cmake --preset dev
cmake --build release/build --config Release
```
