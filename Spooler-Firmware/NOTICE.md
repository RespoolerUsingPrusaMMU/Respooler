# NOTICE

## Prusa Spooler Firmware

Copyright (C) 2026 Andre Pruitt  
Contact: andre@pruittfamily.com

This project is licensed under the GNU General Public License, version 3, or
(at your option) any later version (`GPL-3.0-or-later`). The complete license
text is provided in the `LICENSE` file.

## Relationship to Prusa Research

This project targets the ATmega32U4-based controller and hardware architecture
used by the Original Prusa MMU2S/MMU3 family and was developed with reference
to publicly available Prusa MMU firmware, board behavior, pin assignments, and
TMC2130 usage.

The current spooler firmware contains its own application logic, TMC2130 driver
wrapper, and MMU hardware-abstraction implementation. It does not link the Prusa
MMU firmware HAL object files into the spooler executable. The documented build
process may use the AVR GCC toolchain distributed with the Prusa firmware source
tree as a convenient compiler toolchain.

Prusa Research a.s. is not the author, maintainer, sponsor, or endorser of this
spooler firmware unless Prusa Research explicitly states otherwise.

## Commercial use

The GPL permits commercial use. A person or company may, for example, build and
sell hardware with this firmware installed and may charge for the hardware,
firmware distribution, support, installation, or related services.

When GPL-covered firmware is distributed, including distribution in object-code
form in a product, the distributor must comply with the applicable GPL terms.
Depending on the form of distribution, these obligations can include preserving
copyright and license notices, providing the corresponding source code or a
GPL-compliant source offer, licensing covered modifications under the GPL, and
providing installation information when GPLv3 requires it for a User Product.
See the `LICENSE` file for the controlling terms.

## Trademarks, logos, graphics, and product design

The GPL license applies to software covered by that license. It does not by
itself grant permission to use third-party trademarks, logos, graphics,
industrial designs, product appearance, or other material that may be protected
or separately licensed.

`Original Prusa`, `Original Prusa MMU2S`, and `Original Prusa MMU3` are names
and/or trademarks associated with Prusa Research a.s. A commercial product based
on this firmware should not imply that it is manufactured, sponsored, approved,
or endorsed by Prusa Research unless the seller has permission to make that
claim. Commercial manufacturers should use their own branding, graphics, and
product design unless they separately obtain the rights required to use Prusa
assets.

## No warranty

This software is provided under the warranty disclaimer and limitation-of-
liability provisions of the GNU General Public License. See `LICENSE` for the
complete terms.
