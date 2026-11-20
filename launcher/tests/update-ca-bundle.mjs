// WiiUNX2 GPL-3.0-or-later. Guards the updater's bundled GitHub trust anchors and TLS policy.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {X509Certificate} from 'node:crypto';
import {test} from 'node:test';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const bundle=fs.readFileSync(path.join(root,'launcher/src/utils/UpdateCaBundle.h'),'utf8');
const pemCertificates=[...bundle.matchAll(/-----BEGIN CERTIFICATE-----[\s\S]+?-----END CERTIFICATE-----/g)];
const expected=[
  {name:'ISRG Root X1',subject:/CN=ISRG Root X1/,expiry:Date.UTC(2035,5,4)},
  {name:'USERTrust ECC Certification Authority',subject:/CN=USERTrust ECC Certification Authority/,expiry:Date.UTC(2038,0,18)},
];

test('updater bundles current GitHub trust anchors without disabling TLS verification',()=>{
  assert.equal(pemCertificates.length,expected.length);
  for(let i=0;i<expected.length;i++) {
    const certificate=new X509Certificate(pemCertificates[i][0]);
    assert.match(certificate.subject,expected[i].subject,expected[i].name);
    assert.equal(certificate.ca,true,`${expected[i].name} must be a CA`);
    assert.ok(Date.parse(certificate.validTo)>=expected[i].expiry,`${expected[i].name} validity unexpectedly changed`);
  }
  const service=fs.readFileSync(path.join(root,'launcher/src/utils/UpdateService.cpp'),'utf8');
  assert.match(service,/CURLOPT_SSL_VERIFYPEER,1L/);
  assert.match(service,/CURLOPT_SSL_VERIFYHOST,2L/);
  assert.match(service,/CURLOPT_CAINFO_BLOB/);
});
