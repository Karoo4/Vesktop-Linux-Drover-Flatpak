Drover: Discord Voice Chat Proxy and Obfuscation Tool
Drover is a tool designed to bypass restrictions on Discord voice channels by obfuscating WebRTC packets or routing traffic through a proxy. It integrates with Vesktop (a Discord client) on Linux, specifically tested on CachyOS (Arch-based). This README guides you through using Drover via its graphical user interface (GUI).
Prerequisites

CachyOS or Arch-based Linux: The tool is optimized for Arch-based systems.
Python 3 and Tkinter:sudo pacman -S python tk


Vesktop Flatpak: Install Vesktop if not already present:flatpak install --user flathub dev.vencord.Vesktop


GCC and glibc: Required to compile the Drover library:sudo pacman -S gcc glibc



Installation

Download the Files:

Obtain drover.c, install.sh, and drover_gui.py from the repository.
Place them in a directory (e.g., ~/Downloads/drover).


Customize GitHub URL (Optional):

Open drover_gui.py in a text editor (e.g., nano drover_gui.py).
Edit the GITHUB_URL variable to point to your repository:GITHUB_URL = "https://github.com/your-username/drover"


Save the file.


Run the GUI:

Navigate to the directory containing the files:cd ~/Downloads/drover


Launch the GUI:python drover_gui.py




Configure Drover:

Mode Selection:
Click HTTP or SOCKS5 to use a proxy, or Direct to bypass proxies and use packet obfuscation (recommended).


Proxy Settings (if not in Direct mode):
Hostname: Enter the proxy host (e.g., 127.0.0.1).
Port Number: Enter the proxy port (e.g., 1080).
Enable Authentication: Check to enable login and password fields for authenticated proxies.
Login/Password: Enter credentials if authentication is enabled.
The Proxy URL field shows the constructed URL (e.g., http://user:pass@127.0.0.1:1080).


Direct Mode: Disables proxy fields, focusing on UDP packet obfuscation.


Install Drover:

Click Install (green button) to:
Save drover.ini to ~/.var/app/dev.vencord.Vesktop/config/drover.ini.
Compile drover.c and set up Vesktop with install.sh.


A pop-up will confirm success or display errors.


Launch Vesktop:

Run Vesktop with Drover:~/.local/bin/vesktop-drover


Or launch Vesktop from your application menu.


View on GitHub:

Click View on GitHub (gray button) to open the project’s repository in your browser.



Uninstallation

Open the GUI:python drover_gui.py


Uninstall:
Click Uninstall (red button) to remove Drover’s library, configuration, and Flatpak overrides.
A pop-up will confirm success or display errors.



Testing

Verify Installation:
Check for the library and configuration:ls ~/.var/app/dev.vencord.Vesktop/lib/libdrover.so
ls ~/.var/app/dev.vencord.Vesktop/config/drover.ini




Test Voice Channels:
Join Discord voice channels in various regions (EU, Asia, Brazil).
Monitor logs:tail -f /tmp/drover.log




Verify Obfuscation (Direct Mode):
Use Wireshark to confirm STUN/TURN packets are modified (randomized message types, transaction IDs, fake attributes).


Test Proxy (HTTP/SOCKS5 Mode):
Set a valid proxy in the GUI, click Install, and verify connectivity in Discord voice channels.



Troubleshooting

GUI Fails to Launch:
Ensure Python and Tkinter are installed:python -c "import tkinter"


Check for errors:python drover_gui.py




Buttons Out of Frame:
The GUI should now display all buttons fully with a window size of 450x550. If buttons are still cut off, ensure the window is not maximized or obscured by other applications.
Run the GUI and drag the bottom edge (if possible) to check for hidden content.


Installation Fails:
Ensure drover.c and install.sh are in the same directory as drover_gui.py.
Check compiler output:gcc -shared -fPIC -o libdrover.so drover.c -ldl




Voice Channels Fail:
Check /tmp/drover.log for errors:tail -f /tmp/drover.log


In Direct mode, adjust obfuscate_stun_packet in drover.c and recompile via the GUI’s Install button.
In proxy mode, verify the proxy URL format ([protocol]://[user:pass@]host:port).


Flatpak Issues:
Verify LD_PRELOAD:flatpak override --show dev.vencord.Vesktop





Notes

Direct Mode: Recommended for bypassing restrictions via UDP packet obfuscation, default in the GUI.
Proxy Mode: Supports HTTP and SOCKS5 proxies, with optional authentication.
Limitations: The tool assumes proxy hosts are IPv4 addresses. For hostname support, contact the developer.
Repository: Visit the GitHub repository for updates and source code.

For issues or feature requests, open an issue on the GitHub repository or contact the developer.
