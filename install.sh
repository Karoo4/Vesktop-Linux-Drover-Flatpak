#!/bin/bash

# Installer script for Discord Drover with Vesktop Flatpak on Linux
# Usage: ./install.sh [install|uninstall]

set -e

VESTOP_CONFIG="$HOME/.var/app/dev.vencord.Vesktop/config"
LIB_DIR="$HOME/.var/app/dev.vencord.Vesktop/lib"
LIB_PATH="$LIB_DIR/libdrover.so"
CONFIG_PATH="$VESTOP_CONFIG/drover.ini"
WRAPPER_SCRIPT="$HOME/.local/bin/vesktop-drover"

# Compile the shared library
compile() {
    echo "Compiling drover.c..."
    gcc -shared -fPIC -o libdrover.so drover.c -ldl
    mkdir -p "$LIB_DIR"
    mv libdrover.so "$LIB_PATH"
}

# Install function
install() {
    # Check if gcc is installed
    if ! command -v gcc >/dev/null 2>&1; then
        echo "gcc is not installed. Please install it first."
        echo "On Ubuntu/Debian, run: sudo apt-get install gcc"
        exit 1
    fi

    # Check if Vesktop Flatpak is installed
    if ! flatpak list | grep -q dev.vencord.Vesktop; then
        echo "Vesktop Flatpak not found. Install it with: flatpak install flathub dev.vencord.Vesktop"
        exit 1
    fi

    # Compile and install the library
    compile

    # Create default configuration
    mkdir -p "$VESTOP_CONFIG"
    cat > "$CONFIG_PATH" << EOF
[drover]
proxy = http://127.0.0.1:1080
use-nekobox-proxy = 0
nekobox-proxy = http://127.0.0.1:2080
direct-mode = 1
EOF

    # Grant Flatpak permissions
    flatpak override --user --filesystem="$LIB_DIR" dev.vencord.Vesktop
    flatpak override --user --env=LD_PRELOAD="$LIB_PATH" dev.vencord.Vesktop

    # Create wrapper script
    mkdir -p "$HOME/.local/bin"
    cat > "$WRAPPER_SCRIPT" << EOF
#!/bin/bash
flatpak run --env=LD_PRELOAD="$LIB_PATH" dev.vencord.Vesktop
EOF
    chmod +x "$WRAPPER_SCRIPT"

    echo "Installed drover to $LIB_PATH"
    echo "Created configuration at $CONFIG_PATH"
    echo "Flatpak permissions updated for dev.vencord.Vesktop"
    echo "To run Vesktop with drover, use: $WRAPPER_SCRIPT"
}

# Uninstall function
uninstall() {
    if [ -f "$LIB_PATH" ]; then
        rm "$LIB_PATH"
        rmdir --ignore-fail-on-non-empty "$LIB_DIR"
        echo "Removed $LIB_PATH"
    fi
    if [ -f "$CONFIG_PATH" ]; then
        rm "$CONFIG_PATH"
        echo "Removed $CONFIG_PATH"
    fi
    if [ -f "$WRAPPER_SCRIPT" ]; then
        rm "$WRAPPER_SCRIPT"
        echo "Removed $WRAPPER_SCRIPT"
    fi
    flatpak override --user --reset dev.vencord.Vesktop
    echo "Uninstalled drover and reset Flatpak overrides"
}

# Main
case "$1" in
    install)
        install
        ;;
    uninstall)
        uninstall
        ;;
    *)
        echo "Usage: $0 [install|uninstall]"
        exit 1
        ;;
esac
