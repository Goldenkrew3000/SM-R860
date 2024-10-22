# Enabling Wifi AP under Alpine Linux with the standard Modified Kernel

- Load Driver (ifxdhd.so)
- Enable AP using wpa_supplicant: wpa_supplicant -Dnl80211 -iwlan0 -c /root/wpa.conf -B

Example configuration for AP mode under wpa_supplicant (/root/wpa.conf):
ap_scan=2
network={
	ssid="R860-Alpine"
	mode=2
	frequency=2412
	key_mgmt=NONE
}

- Set a static IP on wlan0
ifconfig wlan0 192.168.4.1

- Launch dnsmasq in a foreground, no-fork mode
dnsmasq -k --no-daemon

Example configuration of dnsmasq (/etc/dnsmasq.conf):
interface=wlan0
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
