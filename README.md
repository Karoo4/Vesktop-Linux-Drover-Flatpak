# Vesktop-Linux-Drover-Flatpak
Drover for vesktop flatpak version

# Required Dependencies
You need `gcc` and Vesktop Flatpak. Let’s verify and install them.

**Check `gcc`**:
```bash
gcc --version
```
**Check `Vesktop Flatpak`**:
```bash
flatpak list | grep dev.vencord.Vesktop
```
if missing, install it:
```bash
flatpak install flathub dev.vencord.Vesktop
```


# Installation
* Step 1
```
git clone https://github.com/Karoo4/Vesktop-Linux-Drover-Flatpak.git
```
put this in your home directory and put it into a file with whatever name you like we will be using vesktop-drover as the file name.

* Step 2
```
mkdir -p ~/.local/bin
cat > ~/.local/bin/vesktop-drover << EOF
```
This creates the `vesktop-drover` script in `~/.local/bin` (a standard location for user scripts) and makes it executable.

 **Fix if it’s missing**:
If `~/vesktop-drover` doesn’t exist, the `install.sh` script likely failed. Let’s re-run it after checking prerequisites.

* Step 3
Navigate to your working directory (ie vesktop-drover)
```
cd ~/vesktop-drover
```

* Step 4
Recreate or move the drover.c and install.sh file into this directory

* Step 5
```
chmod +x install.sh
./install.sh install
```
This should:

    Compile drover.c into ~/.var/app/dev.vencord.Vesktop/lib/libdrover.so.
    Create ~/.var/app/dev.vencord.Vesktop/config/drover.ini.
    Create ~/.local/bin/vesktop-drover.
    Set Flatpak permissions.
If you see errors (e.g., “gcc not found” or “Vesktop not found”), follow the instructions in the error message to install missing tools.

* Step 6
# Test running vesktop-drover
```
~/.local/bin/vesktop-drover
```
Note: If you run ~/vesktop-drover and it’s still a directory, you likely created a directory instead of a file. Remove it and use ~/.local/bin/vesktop-drover as created above.
Check for errors: If it says “bash: /home/yourusername/vesktop-drover: Is a directory”:

    Confirm you removed the directory and created the script in ~/.local/bin (see Step 1). If it says “Permission denied”:
```
chmod +x ~/.local/bin/vesktop-drover
```
If vesktop doesn't start, check for errors:
```
~/.local/bin/vesktop-drover 2> error.log
cat error.log
```
and let me know about them.

* Step 7
If the script runs but you need to set up your proxy or voice chat bypass:
# Configuring drover.ini
```
nvim ~/.var/app/dev.vencord.Vesktop/config/drover.ini
```
Update the proxy or enable direct mode:

    For a proxy (e.g., SOCKS5):
```bash
[drover]
proxy = socks5://your.proxy.ip:1080
use-nekobox-proxy = 0
nekobox-proxy = http://127.0.0.1:2080
direct-mode = 0
```
For voice chat bypass without a proxy (e.g., UAE VoIP restrictions):

```bash
[drover]
proxy = http://127.0.0.1:1080
use-nekobox-proxy = 0
nekobox-proxy = http://127.0.0.1:2080
direct-mode = 1
```
Save and exit (:wq).

* Step 8
# Test Vesktop
run again:
```
~/.local/bin/vesktop-drover
```
Check if chat connects through your proxy or if voice chat works (if in a restricted region).

* Step 9
# Make Vesktop Use Drover from the Application Menu (Optional)
If you want to click Vesktop in your application menu (e.g., GNOME, KDE) and have it use Drover:

1. Edit the desktop entry:
```
nvim ~/.local/share/applications/dev.vencord.Vesktop.desktop
```
If it doesn’t exist, copy it:
```
mkdir -p ~/.local/share/applications
cp /var/lib/flatpak/exports/share/applications/dev.vencord.Vesktop.desktop ~/.local/share/applications/
```
Edit the Exec line to:
```
Exec=/home/yourusername/.local/bin/vesktop-drover
```
Replace yourusername with your actual username (echo $USER) Save and exit (:wq)

2. Update desktop database (optional
```
update-desktop-database ~/.local/share/applications
```
3. Test: Open your application menu, click Vesktop, and verify it uses Drover (check chat or voice functionality).

# Troubleshooting
Troubleshooting

Still says “is a directory”:
    Double-check with ls -l ~/.local/bin/vesktop-drover. If it shows drwxr-xr-x, remove it:
```
rm -r ~/.local/bin/vesktop-drover
```
Recreate it as shown in Step 1.

* Script runs but Vesktop fails:

    Check error.log:
```
~/.local/bin/vesktop-drover 2> error.log
cat error.log
```
  
# Drover for Vesktop — Voice Chat Proxy Helper

## 🛠️ Common Errors

### ❌ “Cannot open shared object”

Ensure the file exists:

```bash
ls ~/.var/app/dev.vencord.Vesktop/lib/libdrover.so
```

If missing, re-run the install script:
```bash
./install.sh install
```
❌ “Permission denied”

Run the following commands:
```bash
chmod +x ~/.local/bin/vesktop-drover
flatpak override --user --filesystem=~/.var/app/dev.vencord.Vesktop/lib dev.vencord.Vesktop
```

## ⚠️ Voice Chat Bypass Not Working?

The current UDP manipulation in drover.c is a placeholder.
If you're in the UAE (or a country with similar restrictions), share details and I can customize the sendto hook to make it work.

## 🌐 Proxy Requirements

Ensure your proxy server (e.g., SOCKS5) is running:
```bash
curl --proxy socks5://your.proxy.ip:1080 https://discord.com
```
Example proxy format:
```text
socks5://your.proxy.ip:1080
```
## ⚠️ Terms of Service Warning
 Using Drover with Vesktop violates Discord’s Terms of Service.
 This may result in a ban. Proceed at your own risk.

## 🌍 UAE / Region Notes
If you're located in the UAE, the default direct-mode setup might not be enough.
You may need custom UDP tweaks in WebRTC behavior.

Let me know if voice chat still doesn't work — I can provide a specific sendto hook.
