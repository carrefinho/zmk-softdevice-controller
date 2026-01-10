# ZMK SoftDevice Controller + Connection Subrating

A ZMK Zephyr module that replaces Zephyr's built-in BLE link layer (`BT_LL_SW_SPLIT`) with Nordic's [SoftDevice Controller](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrfxlib/softdevice_controller/README.html), primarily to enable Bluetooth LE Connection Subrating for significant split keyboard power savings. A split central could see a 4x improvement in battery life with subrating enabled.

Note that subrating benefits only apply to the connection between split keyboard parts, mostly the central. Host devices rarely support subrating; even when they do, the power savings by subrating that link are negligible.

> [!WARNING]
> This is very very experimental and the subrating bits are largely LLM coded, use at your own risk.

> [!IMPORTANT]
> nRF52840 is the only supported SoC for the time being. The module only works with current ZMK main branch based on Zephyr 4.1.

## Usage

Add this module to your `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: carrefinho
      url-base: https://github.com/carrefinho
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-feature-softdevice-subrating
      remote: carrefinho
      repo-path: zmk-softdevice-controller
      revision: main
      import: config/west.yml
  self:
    path: config
```

Then enable the SoftDevice controller and subrating using the snippet in your `build.yaml`:

```yaml
---
include:
  - board: xiao_ble
    shield: your_shield
    snippet: softdevice-controller
```

## License

- [LicenseRef-Nordic-5-Clause](https://github.com/nrfconnect/sdk-nrf/blob/main/LICENSE) for code ported from nRF Connect SDK
- MIT for original code
