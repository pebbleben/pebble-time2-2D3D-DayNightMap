/* Replace your existing phone entry point with this file; do not run both. */
var Clay = require('pebble-clay');
var config = require('./config');
var customClay = require('./custom-clay');
var messageKeys = require('message_keys');
var clay = new Clay(config, customClay, { autoHandleEvents: false });

function valueOf(raw, name) {
  var entry = raw[name];
  if (entry && typeof entry === 'object' &&
      Object.prototype.hasOwnProperty.call(entry, 'value')) return entry.value;
  return entry;
}
function projectionKey() {
  var key = messageKeys.MapProjection;
  if (typeof key !== 'number' || key === 0) {
    throw new Error('Register MapProjection with a nonzero unused message-key ID.');
  }
  return key;
}
function timeKeyIsReserved() {
  var collision = false;
  function visit(items) {
    items.forEach(function(item) {
      if (item.items) visit(item.items);
      if (item.messageKey && messageKeys[item.messageKey] === 0) collision = true;
    });
  }
  visit(config);
  return !collision;
}
Pebble.addEventListener('ready', function() {
  console.log('EARTH PHONE ready: globe-fix-1');
  try { console.log('EARTH PHONE projection key=' + projectionKey()); }
  catch (err) { console.log('EARTH PHONE ERROR ' + err.message); }
  if (!timeKeyIsReserved()) {
    console.log('EARTH PHONE ERROR: config uses key 0, reserved by main.c for time sync.');
    return;
  }
  Pebble.sendAppMessage({0: Math.floor(Date.now()/1000)}, function() {
    console.log('EARTH PHONE time sync sent');
  }, function(err) {
    console.log('EARTH PHONE time sync failed: ' + JSON.stringify(err));
  });
});
Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response || e.response === 'CANCELLED') return;
  try {
    var key = projectionKey();
    if (!timeKeyIsReserved()) throw new Error('Config key 0 conflicts with time sync.');
    var raw = clay.getSettings(e.response, false);
    var selected = valueOf(raw, 'MapProjection');
    if (selected !== '0' && selected !== '1' && selected !== 0 && selected !== 1) {
      throw new Error('MapProjection missing/invalid in configuration response. Check config.js import.');
    }
    var dict = clay.getSettings(e.response);
    /* Explicit numeric value, using the SDK-generated key ID on this build. */
    dict[key] = Number(selected);
    console.log('EARTH PHONE sending projection=' + dict[key] + ' key=' + key);
    Pebble.sendAppMessage(dict, function() {
      console.log('EARTH PHONE config acknowledged');
    }, function(err) {
      console.log('EARTH PHONE config failed: ' + JSON.stringify(err));
    });
  } catch (err) {
    console.log('EARTH PHONE ERROR ' + err.message);
  }
});
