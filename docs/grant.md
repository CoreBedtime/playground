# Grant

What it is: A loader utility that patches amfid and spawns the `fangs` tracer.

Why it is: Temporarily disables Apple code signature checks so unsigned code can be loaded.

Role:
1. Locates amfid.
2. Injects a trampoline to return true for validation checks.
3. Spawns fangs.
4. Restores amfid.
5. Returns fangs exit status.

How it is used: Automatically executed to spawn fangs with elevated privileges. Requires root access and Mach API entitlements.

Optional: No.
