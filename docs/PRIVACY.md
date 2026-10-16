**English** | [Español](PRIVACY.es.md)

# Configure your implementation's notice

Each person or organization adapting, implementing or distributing Huskey Pocket is responsible for writing, publishing, maintaining and providing their implementation's privacy notice, as well as terms of use where appropriate. Repository authors do not manage this on behalf of third parties.

## File to edit

Edit [privacy_notice.h](../firmware/password_manager/privacy_notice.h):

| Value | Purpose |
| --- | --- |
| `HP_PRIVACY_TEXT_ES` / `HP_PRIVACY_TEXT_EN` | Full notice in each language |
| `HP_PRIVACY_ACK_ES` / `HP_PRIVACY_ACK_EN` | Onboarding checkbox text |
| `HP_PRIVACY_FOOTER_ES` / `HP_PRIVACY_FOOTER_EN` | Settings notice footer |
| `HP_PRIVACY_NOTICE_VERSION` | Notice version identifier |

Onboarding and **Settings → Privacy notice** display the same full text. No duplication in `i18n.h` or the screens is needed. Use C++ strings with `\n\n` between paragraphs and `\"` for embedded quotes. Check font coverage when adding characters not covered by existing fonts.

## Content to define

Identify the party responsible for your implementation, contact details and channels providing the full notice. Describe how your version actually handles data, including your modifications, transfers through USB/BLE, QR, backups and exports, and any additional services you integrate. Adapt the content to your use context and applicable requirements.

The included text is a **notice not configured** message, not a ready-to-publish policy or consent template. Do not attribute your distribution's obligations, commercial warranties or service data handling to the repository authors.

## Acknowledgement and version

The default checkbox reads “I have read this notice”. Onboarding stores `legal_ok` and the notice version in `legal_ver`, under the `vprefs` NVS namespace. This records a local reading acknowledgement, not proof of consent or a replacement for your notice.

Changing `HP_PRIVACY_NOTICE_VERSION` **does not reopen onboarding on configured devices**. Implement any notice update or additional acknowledgement flow your distribution requires. Do not reset a device containing real data to test this: resetting erases the vault.

Before distribution, review both languages and verify scrolling through the full text in onboarding and Settings on a test unit. The code remains [MIT licensed](../LICENSE); its license does not serve as a privacy notice.
