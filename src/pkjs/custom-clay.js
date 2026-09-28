module.exports = function(minified) {
  var clayConfig = this;
  var isApplyingTheme = false; 

  var themes = {
    // NEW: Inverted dark silhouette continent theme (Crispest possible on Pebble)
    radar: {
      BackgroundColor: '0x000000', ForegroundColor: '0x55FF55',
      DayLand: '0x000000', DayWater: '0x00FFAA', DayIce: '0xFFFFFF',
      NightLand: '0x005500', NightWater: '0x005555', NightIce: '0x00AA55',
      TimeFont: 'leco', DateFont: 'small'
    },
    // NEW: Warm sepia/parchment tone
    sepia: {
      BackgroundColor: '0x550000', ForegroundColor: '0xFFFFAA',
      DayLand: '0xFFFFAA', DayWater: '0xAA5500', DayIce: '0xFFFFFF',
      NightLand: '0xFFAA55', NightWater: '0x550000', NightIce: '0xAA5555',
      TimeFont: 'serif', DateFont: 'serif'
    },
    // NEW: High contrast solar gold/black
    solar: {
      BackgroundColor: '0x000000', ForegroundColor: '0xFFFF00',
      DayLand: '0xFFFF55', DayWater: '0xAA0000', DayIce: '0xFFFFFF',
      NightLand: '0xFFAA00', NightWater: '0x000000', NightIce: '0xAA5555',
      TimeFont: 'roboto', DateFont: 'gothic'
    },
    // RETUNED: Replaced low-contrast Green/Blue with clean luminance hierarchy
    classic: {
      BackgroundColor: '0x000000', ForegroundColor: '0xFFFFFF',
      DayLand: '0x55FF55', DayWater: '0x0055AA', DayIce: '0xFFFFFF',
      NightLand: '0x00AA00', NightWater: '0x000055', NightIce: '0x55AAAA',
      TimeFont: 'leco', DateFont: 'bitham'
    },
    // NEW: Crimson silhouette — bright red land, black sea
    redHC: {
      BackgroundColor: '0x000000', ForegroundColor: '0xFF5555',
      DayLand: '0xFF5555', DayWater: '0x000000', DayIce: '0xFFFFFF',
      NightLand: '0xFF5555', NightWater: '0x550000', NightIce: '0xFFFFFF',
      TimeFont: 'leco', DateFont: 'bitham'
    },

    // NEW: Amber / ember — gold land, maroon sea
    orangeHC: {
      BackgroundColor: '0x000000', ForegroundColor: '0xFFAA00',
      DayLand: '0xFFAA00', DayWater: '0x000000', DayIce: '0xFFFFFF',
      NightLand: '0xFF5500', NightWater: '0x550000', NightIce: '0xFFFFFF',
      TimeFont: 'leco', DateFont: 'bitham'
    },

    // NEW: Solar — yellow land, deep navy sea (max separation)
    yellowHC: {
      BackgroundColor: '0x000000', ForegroundColor: '0xFFFF55',
      DayLand: '0xFFFF55', DayWater: '0x000055', DayIce: '0xFFFFFF',
      NightLand: '0xAAAA00', NightWater: '0x000000', NightIce: '0xFFFFAA',
      TimeFont: 'leco', DateFont: 'bitham'
    },

    // NEW: Verdant — mint land, navy sea
    greenHC: {
      BackgroundColor: '0x000000', ForegroundColor: '0x55FF55',
      DayLand: '0x55FF55', DayWater: '0x000055', DayIce: '0xFFFFFF',
      NightLand: '0x00AA00', NightWater: '0x000000', NightIce: '0xFFFFFF',
      TimeFont: 'leco', DateFont: 'bitham'
    },

    // NEW: Azure — pale blue land, near-black navy sea
    //      (NEVER use 0x0000FF here — it is L=0.072, CR 1.44 on black)
    blueHC: {
      BackgroundColor: '0x000000', ForegroundColor: '0x55FFFF',
      DayLand: '0x55AAFF', DayWater: '0x000055', DayIce: '0xFFFFFF',
      NightLand: '0x00AAFF', NightWater: '0x000000', NightIce: '0xFFFFFF',
      TimeFont: 'leco', DateFont: 'bitham'
    },    
//     classic: {
//       BackgroundColor: '0x000000', ForegroundColor: '0xFFFFFF',
//       DayLand: '0x00FF00', DayWater: '0x0055FF', DayIce: '0xFFFFFF',
//       NightLand: '0x005500', NightWater: '0x000055', NightIce: '0x555555',
//       TimeFont: 'leco', DateFont: 'bitham'
//     },
    terminal: {
      BackgroundColor: '0x000000', ForegroundColor: '0x00FF00',
      DayLand: '0x00FF00', DayWater: '0x000000', DayIce: '0x00AA00',
      NightLand: '0x005500', NightWater: '0x000000', NightIce: '0x005500',
      TimeFont: 'leco', DateFont: 'small'
    },
    ocean: {
      BackgroundColor: '0x000055', ForegroundColor: '0x55FFFF',
      DayLand: '0x55FFFF', DayWater: '0x0055FF', DayIce: '0xFFFFFF',
      NightLand: '0x0055AA', NightWater: '0x000055', NightIce: '0xAAAAAA',
      TimeFont: 'roboto', DateFont: 'gothic'
    },
    mars: {
      BackgroundColor: '0x550000', ForegroundColor: '0xFFFFAA',
      DayLand: '0xFFAA00', DayWater: '0xAA5500', DayIce: '0xFFFFFF',
      NightLand: '0x550000', NightWater: '0x000000', NightIce: '0xAA5555'
    },
    cyberpunk: {
      BackgroundColor: '0x000055', ForegroundColor: '0xFF00FF',
      DayLand: '0xFF00FF', DayWater: '0x00FFFF', DayIce: '0xFFFFFF',
      NightLand: '0x550055', NightWater: '0x005555', NightIce: '0xAAAAFF',
      TimeFont: 'bitham', DateFont: 'bitham'
    },
    bw: {
      BackgroundColor: '0x000000', ForegroundColor: '0xFFFFFF',
      DayLand: '0xFFFFFF', DayWater: '0x555555', DayIce: '0xAAAAAA',
      NightLand: '0x555555', NightWater: '0x000000', NightIce: '0x555555',
      TimeFont: 'leco', DateFont: 'gothic'
    },
    miami: {
      BackgroundColor: '0x000055',ForegroundColor: '0x55FFFF',    
      DayLand: '0xFF55AA',DayWater: '0x00AAFF',DayIce: '0xFFFFFF',    
      NightLand: '0xAA0055',NightWater: '0x000055',NightIce: '0xAA55FF',
      TimeFont: 'leco', DateFont: 'gothic'
    },    
    aurora: {
      BackgroundColor: '0x000055',ForegroundColor: '0xAAFFAA',    
      DayLand: '0x55FFAA',DayWater: '0x0055AA',DayIce: '0xFFFFFF',    
      NightLand: '0x00AA55',NightWater: '0x000000',NightIce: '0x5555AA',
      TimeFont: 'leco', DateFont: 'gothic'
    },    
    cherry: {
      BackgroundColor: '0x550000',ForegroundColor: '0xFFFFAA',
      DayLand: '0xFF5555',DayWater: '0x0055AA',DayIce: '0xFFFFAA',    
      NightLand: '0xAA0000',NightWater: '0x000000',NightIce: '0x555555'
    },    
    candy: {
      BackgroundColor: '0x550055',ForegroundColor: '0xFFFFFF',    
      DayLand: '0xFFAAAA',DayWater: '0x55FFFF',DayIce: '0xFFFFFF',    
      NightLand: '0xAA55AA',NightWater: '0x0055AA',NightIce: '0xAA55FF'
    },    
    gold: {
      BackgroundColor: '0x000000',ForegroundColor: '0xFFFF55',    
      DayLand: '0xFFAA00',DayWater: '0x005555',DayIce: '0xFFFFAA',    
      NightLand: '0xAA5500',NightWater: '0x000000',NightIce: '0x555555',
      TimeFont: 'serif', DateFont: 'serif'
    },    
    arctic: {
      BackgroundColor: '0x000055',ForegroundColor: '0xFFFFFF',    
      DayLand: '0xAAFFFF',DayWater: '0x0055FF',DayIce: '0xFFFFFF',    
      NightLand: '0x0055AA',NightWater: '0x000055',NightIce: '0xAAAAFF',
      TimeFont: 'leco', DateFont: 'gothic'
    },    
    jungle: {
      BackgroundColor: '0x000000',ForegroundColor: '0xAAFF55',    
      DayLand: '0x55FF55',DayWater: '0x0055AA',DayIce: '0xFFFFFF',    
      NightLand: '0x005500',NightWater: '0x000000',NightIce: '0x555555',
      TimeFont: 'leco', DateFont: 'gothic'
    },    
    arcade: {
      BackgroundColor: '0x000000',ForegroundColor: '0x55FF55',    
      DayLand: '0xFFFF00',DayWater: '0x0055FF',DayIce: '0xFFFFFF',    
      NightLand: '0xFF0055',NightWater: '0x000055',NightIce: '0xAA00FF',
      TimeFont: 'leco', DateFont: 'gothic'
    },    
    atlas: {
      BackgroundColor: '0xAA5500',ForegroundColor: '0xFFFFAA',    
      DayLand: '0xFFAA55',DayWater: '0x0055AA',DayIce: '0xFFFFAA',    
      NightLand: '0x555500',NightWater: '0x000055',NightIce: '0xAAAA55',
      TimeFont: 'leco', DateFont: 'serif'
    },
    tropical: {
      BackgroundColor: '0x005555',ForegroundColor: '0xFFFF55',    
      DayLand: '0xFF5555',DayWater: '0x00AAFF',DayIce: '0xFFFFFF',    
      NightLand: '0x55AA00',NightWater: '0x000055',NightIce: '0x5555AA',
      TimeFont: 'leco', DateFont: 'gothic'
    }
  };

  function toggleVisibility() {
    var projection = clayConfig.getItemByMessageKey('MapProjection');
    var latitude = clayConfig.getItemByMessageKey('LatitudeOffset');
    if (String(projection.get()) === '1') latitude.show();
    else latitude.hide();
    if (String(clayConfig.getItemByMessageKey('CenterFocus').get()) === '0') {
      clayConfig.getItemByMessageKey('LongitudeOffset').show();
    } else {
      clayConfig.getItemByMessageKey('LongitudeOffset').hide();
    }

    if (String(clayConfig.getItemByMessageKey('DayMap').get()) === '2') {
      clayConfig.getItemByMessageKey('DayLand').show();
      clayConfig.getItemByMessageKey('DayWater').show();
      clayConfig.getItemByMessageKey('DayIce').show();
    } else {
      clayConfig.getItemByMessageKey('DayLand').hide();
      clayConfig.getItemByMessageKey('DayWater').hide();
      clayConfig.getItemByMessageKey('DayIce').hide();
    }

    if (String(clayConfig.getItemByMessageKey('NightMap').get()) === '2') {
      clayConfig.getItemByMessageKey('NightLand').show();
      clayConfig.getItemByMessageKey('NightWater').show();
      clayConfig.getItemByMessageKey('NightIce').show();
    } else {
      clayConfig.getItemByMessageKey('NightLand').hide();
      clayConfig.getItemByMessageKey('NightWater').hide();
      clayConfig.getItemByMessageKey('NightIce').hide();
    }
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var themeSelect = clayConfig.getItemByMessageKey('Theme');
    var focusSelect = clayConfig.getItemByMessageKey('CenterFocus');
    var dayMapSelect = clayConfig.getItemByMessageKey('DayMap');
    var nightMapSelect = clayConfig.getItemByMessageKey('NightMap');

    var settingKeys = [
      'BackgroundColor', 'ForegroundColor', 
      'DayLand', 'DayWater', 'DayIce', 
      'NightLand', 'NightWater', 'NightIce',
      'TimeFont', 'DateFont'
    ];

    toggleVisibility();

    clayConfig.getItemByMessageKey('MapProjection').on('change', toggleVisibility);
    focusSelect.on('change', toggleVisibility);
    dayMapSelect.on('change', toggleVisibility);
    nightMapSelect.on('change', toggleVisibility);

    function resetToCustom() {
      if (isApplyingTheme) return; 
      if (themeSelect.get() !== 'custom') {
        themeSelect.set('custom');
      }
    }

    settingKeys.forEach(function(key) {
      clayConfig.getItemByMessageKey(key).on('change', resetToCustom);
    });

    themeSelect.on('change', function() {
      var themeName = themeSelect.get();
      if (themeName !== 'custom' && themes[themeName]) {
        var preset = themes[themeName];
        isApplyingTheme = true; 
        
        settingKeys.forEach(function(key) {
          if (preset[key]) {
            clayConfig.getItemByMessageKey(key).set(preset[key]);
          }
        });
        
        dayMapSelect.set('2');
        nightMapSelect.set('2');
        toggleVisibility();
        
        isApplyingTheme = false; 
      }
    });
  });
};

// 260923 pre new high contrast
// module.exports = function(minified) {
//   var clayConfig = this;
//   var isApplyingTheme = false; 

//   var themes = {
//     classic: {
//       BackgroundColor: '0x000000', ForegroundColor: '0xFFFFFF',
//       DayLand: '0x00FF00', DayWater: '0x0055FF', DayIce: '0xFFFFFF',
//       NightLand: '0x005500', NightWater: '0x000055', NightIce: '0x555555',
//       TimeFont: 'leco', DateFont: 'bitham'
//     },
//     terminal: {
//       BackgroundColor: '0x000000', ForegroundColor: '0x00FF00',
//       DayLand: '0x00FF00', DayWater: '0x000000', DayIce: '0x00AA00',
//       NightLand: '0x005500', NightWater: '0x000000', NightIce: '0x005500',
//       TimeFont: 'leco', DateFont: 'small'
//     },
//     ocean: {
//       BackgroundColor: '0x000055', ForegroundColor: '0x55FFFF',
//       DayLand: '0x55FFFF', DayWater: '0x0055FF', DayIce: '0xFFFFFF',
//       NightLand: '0x0055AA', NightWater: '0x000055', NightIce: '0xAAAAAA',
//       TimeFont: 'roboto', DateFont: 'gothic'
//     },
//     mars: {
//       BackgroundColor: '0x550000', ForegroundColor: '0xFFFFAA',
//       DayLand: '0xFFAA00', DayWater: '0xAA5500', DayIce: '0xFFFFFF',
//       NightLand: '0x550000', NightWater: '0x000000', NightIce: '0xAA5555'
//     },
//     cyberpunk: {
//       BackgroundColor: '0x000055', ForegroundColor: '0xFF00FF',
//       DayLand: '0xFF00FF', DayWater: '0x00FFFF', DayIce: '0xFFFFFF',
//       NightLand: '0x550055', NightWater: '0x005555', NightIce: '0xAAAAFF',
//       TimeFont: 'bitham', DateFont: 'bitham'
//     },
//     bw: {
//       BackgroundColor: '0x000000', ForegroundColor: '0xFFFFFF',
//       DayLand: '0xFFFFFF', DayWater: '0x555555', DayIce: '0xAAAAAA',
//       NightLand: '0x555555', NightWater: '0x000000', NightIce: '0x555555'
//     },
//     miami: {
//       BackgroundColor: '0x000055',ForegroundColor: '0x55FFFF',    
//       DayLand: '0xFF55AA',DayWater: '0x00AAFF',DayIce: '0xFFFFFF',    
//       NightLand: '0xAA0055',NightWater: '0x000055',NightIce: '0xAA55FF'
//     },    
//     aurora: {
//       BackgroundColor: '0x000055',ForegroundColor: '0xAAFFAA',    
//       DayLand: '0x55FFAA',DayWater: '0x0055AA',DayIce: '0xFFFFFF',    
//       NightLand: '0x00AA55',NightWater: '0x000000',NightIce: '0x5555AA'
//     },    
//     cherry: {
//       BackgroundColor: '0x550000',ForegroundColor: '0xFFFFAA',
//       DayLand: '0xFF5555',DayWater: '0x0055AA',DayIce: '0xFFFFAA',    
//       NightLand: '0xAA0000',NightWater: '0x000000',NightIce: '0x555555'
//     },    
//     candy: {
//       BackgroundColor: '0x550055',ForegroundColor: '0xFFFFFF',    
//       DayLand: '0xFFAAAA',DayWater: '0x55FFFF',DayIce: '0xFFFFFF',    
//       NightLand: '0xAA55AA',NightWater: '0x0055AA',NightIce: '0xAA55FF'
//     },    
//     gold: {
//       BackgroundColor: '0x000000',ForegroundColor: '0xFFFF55',    
//       DayLand: '0xFFAA00',DayWater: '0x005555',DayIce: '0xFFFFAA',    
//       NightLand: '0xAA5500',NightWater: '0x000000',NightIce: '0x555555',
//       TimeFont: 'serif', DateFont: 'serif'
//     },    
//     arctic: {
//       BackgroundColor: '0x000055',ForegroundColor: '0xFFFFFF',    
//       DayLand: '0xAAFFFF',DayWater: '0x0055FF',DayIce: '0xFFFFFF',    
//       NightLand: '0x0055AA',NightWater: '0x000055',NightIce: '0xAAAAFF'
//     },    
//     jungle: {
//       BackgroundColor: '0x000000',ForegroundColor: '0xAAFF55',    
//       DayLand: '0x55FF55',DayWater: '0x0055AA',DayIce: '0xFFFFFF',    
//       NightLand: '0x005500',NightWater: '0x000000',NightIce: '0x555555'
//     },    
//     arcade: {
//       BackgroundColor: '0x000000',ForegroundColor: '0x55FF55',    
//       DayLand: '0xFFFF00',DayWater: '0x0055FF',DayIce: '0xFFFFFF',    
//       NightLand: '0xFF0055',NightWater: '0x000055',NightIce: '0xAA00FF',
//       TimeFont: 'leco', DateFont: 'gothic'
//     },    
//     atlas: {
//       BackgroundColor: '0xAA5500',ForegroundColor: '0xFFFFAA',    
//       DayLand: '0xFFAA55',DayWater: '0x0055AA',DayIce: '0xFFFFAA',    
//       NightLand: '0x555500',NightWater: '0x000055',NightIce: '0xAAAA55',
//       TimeFont: 'serif', DateFont: 'serif'
//     },
//     tropical: {
//       BackgroundColor: '0x005555',ForegroundColor: '0xFFFF55',    
//       DayLand: '0xFF5555',DayWater: '0x00AAFF',DayIce: '0xFFFFFF',    
//       NightLand: '0x55AA00',NightWater: '0x000055',NightIce: '0x5555AA'
//     }
//   };

//   function toggleVisibility() {
//     if (clayConfig.getItemByMessageKey('CenterFocus').get() === '0') {
//       clayConfig.getItemByMessageKey('LongitudeOffset').show();
//     } else {
//       clayConfig.getItemByMessageKey('LongitudeOffset').hide();
//     }

//     if (clayConfig.getItemByMessageKey('DayMap').get() === '2') {
//       clayConfig.getItemByMessageKey('DayLand').show();
//       clayConfig.getItemByMessageKey('DayWater').show();
//       clayConfig.getItemByMessageKey('DayIce').show();
//     } else {
//       clayConfig.getItemByMessageKey('DayLand').hide();
//       clayConfig.getItemByMessageKey('DayWater').hide();
//       clayConfig.getItemByMessageKey('DayIce').hide();
//     }

//     if (clayConfig.getItemByMessageKey('NightMap').get() === '2') {
//       clayConfig.getItemByMessageKey('NightLand').show();
//       clayConfig.getItemByMessageKey('NightWater').show();
//       clayConfig.getItemByMessageKey('NightIce').show();
//     } else {
//       clayConfig.getItemByMessageKey('NightLand').hide();
//       clayConfig.getItemByMessageKey('NightWater').hide();
//       clayConfig.getItemByMessageKey('NightIce').hide();
//     }
//   }

//   clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
//     var themeSelect = clayConfig.getItemByMessageKey('Theme');
//     var focusSelect = clayConfig.getItemByMessageKey('CenterFocus');
//     var dayMapSelect = clayConfig.getItemByMessageKey('DayMap');
//     var nightMapSelect = clayConfig.getItemByMessageKey('NightMap');

//     var settingKeys = [
//       'BackgroundColor', 'ForegroundColor', 
//       'DayLand', 'DayWater', 'DayIce', 
//       'NightLand', 'NightWater', 'NightIce',
//       'TimeFont', 'DateFont'
//     ];

//     toggleVisibility();

//     focusSelect.on('change', toggleVisibility);
//     dayMapSelect.on('change', toggleVisibility);
//     nightMapSelect.on('change', toggleVisibility);

//     function resetToCustom() {
//       if (isApplyingTheme) return; 
//       if (themeSelect.get() !== 'custom') {
//         themeSelect.set('custom');
//       }
//     }

//     settingKeys.forEach(function(key) {
//       clayConfig.getItemByMessageKey(key).on('change', resetToCustom);
//     });

//     themeSelect.on('change', function() {
//       var themeName = themeSelect.get();
//       if (themeName !== 'custom' && themes[themeName]) {
//         var preset = themes[themeName];
//         isApplyingTheme = true; 
        
//         settingKeys.forEach(function(key) {
//           if (preset[key]) {
//             clayConfig.getItemByMessageKey(key).set(preset[key]);
//           }
//         });
        
//         dayMapSelect.set('2');
//         nightMapSelect.set('2');
//         toggleVisibility();
        
//         isApplyingTheme = false; 
//       }
//     });
//   });
// };