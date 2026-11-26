// WiiUNX2 GPL-3.0-or-later. Source-wiring checks for installed and disc titles.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const read=file=>fs.readFileSync(path.join(root,file),'utf8');

test('installed title enumeration excludes the optical drive',()=>{
  const source=read('launcher/src/game/GameList.cpp');
  assert.match(source,/MCP_DEVICE_TYPE_MLC/);
  assert.match(source,/MCP_DEVICE_TYPE_USB/);
  assert.match(source,/MCP_TitleListByAppAndDeviceType/);
  assert.doesNotMatch(source,/MCP_TitleListByAppType\s*\(/);
});

test('disc titles use a separate icon instead of a name suffix',()=>{
  const source=read('launcher/src/menu/MainWindow.cpp');
  assert.match(source,/discIcon=asset\("nxDisc\.png"\)/);
  assert.match(source,/selectedTile\(\)\.id==discTileId&&discIcon/);
  assert.match(source,/iconSize=30\.f/);
  assert.doesNotMatch(source,/\+" \(Disc\)"/);
  const asset=fs.readFileSync(path.join(root,'launcher/data/images/nxDisc.png'));
  assert.deepEqual([...asset.subarray(0,8)],[137,80,78,71,13,10,26,10]);
  assert.equal(asset.readUInt32BE(16),48);
  assert.equal(asset.readUInt32BE(20),48);
});

test('empty ODD results trigger a one-time ID-free AUTO-list diagnostic',()=>{
  const source=read('launcher/src/menu/MainWindow.cpp');
  const diagnostic=source.split('static void logAutoDiscCandidates(int handle) {')[1]?.split('\nstatic nx::DiscSample readDisc')[0];
  assert.ok(diagnostic);
  assert.match(diagnostic,/MCP_DEVICE_TYPE_AUTO/);
  assert.match(diagnostic,/MCP_TitleListByDeviceType/);
  assert.match(diagnostic,/MCP_APP_TYPE_GAME_WII/);
  assert.match(diagnostic,/indexedDevice/);
  assert.doesNotMatch(diagnostic,/\.titleId|\.path/);
  assert.match(diagnostic,/constexpr size_t capacity=8192/);
  assert.match(diagnostic,/count>capacity/);
  assert.match(source,/if\(!autoDiagnosticDone\)\s*\{\s*autoDiagnosticDone=true;\s*logAutoDiscCandidates\(handle\);/);
});

test('disc insertion changes Recent ordering without recording a play',()=>{
  const state=read('launcher/src/menu/DiscState.h');
  const source=read('launcher/src/menu/MainWindow.cpp');
  assert.match(state,/recentlyInsertedDiscBefore/);
  assert.match(source,/recentlyInsertedDiscBefore\(aIsDisc,bIsDisc,discIsInserted\)/);
  assert.doesNotMatch(source,/markPlayed\(info->launchId\)/);
});

test('installed game launch permits the system splash path',()=>{
  const source=read('launcher/src/menu/MainWindow.cpp');
  assert.match(source,/SYSLaunchTitle\(id\)/);
  assert.doesNotMatch(source,/_SYSLaunchTitleWithStdArgsInNoSplash/);
});
