# Sarah – Voice Assistant Client

Sarah is a small, fast voice-assistant client written in C++.  
It’s built to live on little headless boxes a Raspberry Pi, an Intel NUC, or anything similar and quietly wait for you to talk.

When you say the wake word, Sarah records your command, sends it off to a backend service (the **Orchestrator**), and then plays whatever audio reply comes back. That’s it.

## How it fits together

The client is just the “ears and mouth.”  
All the heavy stuff — speech-to-text, language model work, text-to-speech — happens on the Orchestrator. Keeping things separate makes the client light and quick, which is great for low-power hardware.

**Data flow looks like this:**

```
Wake word → Record (VAD) → POST to Orchestrator → Receive audio → Play
```

## Features

- Wake-word listening powered by [Porcupine](https://picovoice.ai/platform/porcupine/)
    
- Low-latency audio capture/playback (everything stays in memory)
    
- Voice activity detection so you don’t have to set a fixed recording length
    
- Runs headless as a `systemd` user service; survives reboots and errors
    
- Works nicely with [Tailscale](https://tailscale.com/) for easy, secure networking
    

## Getting started (Arch Linux)

These steps assume you have a headless Arch box with a mic and internet.

### 1. Grab the code

```bash
git clone https://github.com/almimony75/client.git
cd client
```

### 2. Configure it

Configuration is now handled in an external file. Edit `client.conf` in the project's root directory.


### 3. Build and install

```bash
sudo ./setup.sh
```

That script pulls dependencies, builds everything, and drops the service file in the right place.

### 4. Start the service

Run these as your **normal user** (not root):

```bash
systemctl --user daemon-reload
systemctl --user enable --now sarah-client.service
```

Sarah should now be running quietly in the background.

## Managing it

Because Sarah runs as a _user_ service, always use `systemctl --user`:

|What you want|Command|
|---|---|
|See if it’s alive|`systemctl --user status sarah-client.service`|
|Watch logs|`journalctl --user -u sarah-client.service -f`|
|Restart after editing code|`systemctl --user restart sarah-client.service`|
|Stop it|`systemctl --user stop sarah-client.service`|

## Removing it

1. Run the uninstall script:
    

```bash
sudo ./uninstall.sh
```

2. Then, as your user:
    

```bash
systemctl --user stop sarah-client.service
systemctl --user disable sarah-client.service
systemctl --user daemon-reload
```

That cleans up the service and removes the files.

## Hacking on it

If you tweak the code, just rebuild and restart:

```bash
./rebuild.sh
```

(A simple `g++` command from `setup.sh` plus  
`systemctl --user restart sarah-client.service` is enough.)

## Tech used

- C++17
    
- Wake word: Picovoice Porcupine
    
- Audio I/O: PortAudio
    
- HTTP client: cpp-httplib
    
- Networking: Tailscale
    
- Runs under `systemd`
    
