local limit=tick()+25
repeat wait(.1) until workspace:FindFirstChild('LibraryAcceptanceMarker') or tick()>limit
assert(workspace:FindFirstChild('LibraryAcceptanceMarker'),'Selected library place was not replicated')
print('[LibraryTest] Actual selected place marker replicated')
