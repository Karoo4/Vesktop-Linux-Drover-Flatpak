# Drover

A Discord voice chat proxy and obfuscation tool that bypasses restrictions on Discord voice channels by obfuscating WebRTC packets or routing traffic through a proxy. Integrates with Vesktop (Discord client) on Linux.

## Features

- **Direct Mode**: UDP packet obfuscation (recommended)
- **Proxy Mode**: HTTP and SOCKS5 proxy support with optional authentication
- **GUI Interface**: Easy-to-use graphical configuration
- **Linux Optimized**: Tested on CachyOS (Arch-based systems)

## Prerequisites

- **Python**: Python 3 with Tkinter
- **Vesktop**: Discord client via Flatpak
- **Build Tools**: GCC and glibc

### Install Dependencies

```bash
# Install Python and Tkinter
sudo pacman -S python tk

# Install Vesktop
flatpak install --user flathub dev.vencord.Vesktop

# Install build tools
sudo pacman -S gcc glibc
```

## Installation

1. **Download Files**
   ```bash
   # Place drover.c, install.sh, and drover_gui.py in a directory
   cd ~/Downloads/drover
   ```

3. **Launch GUI**
   ```bash
   python drover_gui.py
   ```

## Configuration

### Mode Selection
- **Direct**: Packet obfuscation without proxy (recommended for UAE)
- **HTTP/SOCKS5**: Route traffic through proxy

### Proxy Settings (if not Direct mode)
- **Hostname**: Proxy host (e.g., `127.0.0.1`)
- **Port**: Proxy port (e.g., `1080`)
- **Authentication**: Optional login/password for authenticated proxies

### Installation Process
1. Click **Install** (green button)
2. Configuration saved to `~/.var/app/dev.vencord.Vesktop/config/drover.ini`
3. Library compiled and Vesktop configured automatically

## Usage

### Launch Vesktop with Drover
```bash
~/.local/bin/vesktop-drover
```
Or launch from your application menu.

### Testing
```bash
# Verify installation
ls ~/.var/app/dev.vencord.Vesktop/lib/libdrover.so
ls ~/.var/app/dev.vencord.Vesktop/config/drover.ini

# Monitor logs
tail -f /tmp/drover.log
```

## Uninstallation

1. Run the GUI: `python drover_gui.py`
2. Click **Uninstall** (red button)
3. Removes library, configuration, and Flatpak overrides

## Troubleshooting

### GUI Issues
```bash
# Test Tkinter installation
python -c "import tkinter"

# Check for GUI errors
python drover_gui.py
```

### Installation Problems
- Ensure all files (`drover.c`, `install.sh`, `drover_gui.py`) are in the same directory
- Check compiler output if build fails

### Voice Channel Issues
```bash
# Check logs for errors
tail -f /tmp/drover.log

# Verify Flatpak configuration
flatpak override --show dev.vencord.Vesktop
```

## Technical Details

- **Direct Mode**: Modifies STUN/TURN packets (randomized message types, transaction IDs, fake attributes)
- **Proxy Support**: IPv4 addresses only (hostname support on request)
- **GUI Window**: 450x550 pixels, fully responsive

## Support

- **Repository**: Visit GitHub for updates and source code
- **Issues**: Open an issue on the GitHub repository
- **Contact**: Reach out to the developer for feature requests

## Notes

- Direct mode is recommended for most use cases (MAINLY THE UAE)
- Proxy mode supports both HTTP and SOCKS5 with authentication
- Tool was tested on an Arch-Based system
