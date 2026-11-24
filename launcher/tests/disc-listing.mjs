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
  assert.doesNotMatch(source,/\+" \(Disc\)"/);
  const asset=fs.readFileSync(path.join(root,'launcher/data/images/nxDisc.png'));
  assert.deepEqual([...asset.subarray(0,8)],[137,80,78,71,13,10,26,10]);
  assert.equal(asset.readUInt32BE(16),32);
  assert.equal(asset.readUInt32BE(20),32);
});
