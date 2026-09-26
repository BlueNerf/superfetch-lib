# Superfetch
`Superfetch` aka `sysmain` is a service in Windows used for memory management that preloads the apps you use frequently, attempting to make startup times quicker.
## How does this allow us to translate virtual kernel addresses (or any virtual addresses) to physical addresses?
Superfetch exposes some NtQuerySystemInformation apis that give us some PFN information.
We can use SuperfetchPfnQuery and SuperfetchMemoryRangesQuery to get the MemoryRanges and PFNs.

If you want to learn more, I (will VERY SOON and not even faking I will upload something im working on it rn) upload a write-up on https://kryo.rich which you can read and even make your own library with.

## Installation
Simply copy the files in /include/ to your project directory.

## License
Refer to LICENSE file.