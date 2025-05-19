# Bugs to remove and features to add

- The RTC does not seem to work with the standard linux interface
    - More testing:
    Simple RTC program to read /dev/rtc0 in C does work!!
    It logs in dmesg every time it is read, as 's2mpw03-rtc s2m_rtc_read_time'
    Im not sure how the system writes time to it, whether I have to call that function manually but that looks very on time, more experiments needed
    (This will be used for when the watch cannot fetch an internet connection, or fails the internet test on bootups)
- There is a new kernel update.
