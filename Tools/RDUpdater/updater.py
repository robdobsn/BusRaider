from ReleaseMgr import ReleaseMgr

rm = ReleaseMgr("robdobsn", "BusRaider")
rm.refresh()
print(rm.getReleaseList())

