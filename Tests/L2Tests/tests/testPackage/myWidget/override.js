Object.defineProperty(KeyboardEvent.prototype, "keyCode", {
  get: function() {
    // console.log("Key",this.which);
    switch (this.which) {
    case 27: return 8; // map escape to backspace for EntOS US
    default: return this.which;
    }
  }
})

document.overrideLaunchArgs = function( args ) {
  try {
    args.durableAppId = "com.xumo.ipa";
  } catch(e) {
  }
  return args;
}
