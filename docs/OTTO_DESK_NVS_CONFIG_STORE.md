# Otto Desk firmware configuration storage

`main/otto_desk_config_store.*` provides bounded NVS persistence for the Otto Desk configuration.

## API

- `OttoDeskConfigStore::ValidateSafeJson(json)`: checks the payload limit, complete JSON parsing (including trailing-data rejection), duplicate object keys, exact version-1 key sets, field types, fixed board identity and the current fail-closed hardware policy.
- `OttoDeskConfigStore::Save(json)`: validates before writing to NVS namespace `ottdesk`, key `config`, then commits.
- `OttoDeskConfigStore::Load(json)`: reads the stored string and validates it again before returning it.
- `OttoDeskConfigStore::Clear()`: erases the stored configuration key.
- `OttoDeskConfigStore::Decode(json, config)`: decodes an accepted safe document into the runtime configuration structure.

NVS must already be initialized by the application before this class is used. `ESP_ERR_NVS_NOT_FOUND` means no custom configuration has been saved.

## Current hardware policy

The firmware-side validator currently accepts only the safe starter state:

- expected Robot-cua-TRUONG / XiaoZhi v2.5.0 / ESP32-S3-CAM N16R8 identity;
- camera, onboard codec and onboard I/O expander disabled;
- ST7789, INMP441, MAX98357A, servos and PIR disabled;
- every peripheral GPIO assignment null;
- external servo power not marked verified;
- all GPIO conflict and board-pinout policy checks enabled.

The JSON payload limit is 3072 bytes. Unknown keys, duplicate keys, malformed JSON, trailing data and any attempt to change the fixed hardware-disabled state are rejected.

This is deliberately a safe configuration editor/persistence stage, not yet the final configurable hardware schema. Non-null GPIO assignments are not accepted and are not applied to hardware.

## Configuration web service

`main/otto_desk_config_server.*` implements an HTTP configuration page and JSON API:

- `GET /`: browser-based JSON editor with import/export controls;
- `GET /api/config`: reads the stored configuration;
- `POST /api/config`: validates and saves the submitted JSON;
- HTTP port 8080.

The board starts this service when Wi-Fi provisioning mode is entered and stops it when Wi-Fi connects or provisioning exits. It is therefore currently an AP/provisioning-mode setup portal, not an always-on station-mode management server. The service does not apply GPIO assignments or initialize peripheral drivers.

## Startup and remaining integration

The application initializes NVS and loads the saved configuration before normal application startup. A valid document is available to the configuration layer, but the Otto board still returns `DummyAudioCodec` and the default `Board::GetDisplay()` remains `NoDisplay`. `OttoDeskDisplay`, `OttoI2sAudioCodec`, servo and PIR drivers are compiled as separate components but are not yet activated by the board runtime.

Next integration work requires a verified board-specific 40-pin header map and a configuration schema that can safely validate explicit assignments. Until then, hardware activation remains disabled by design.
