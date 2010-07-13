/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons can be installed from XPI files

// The maximum allowable time since install. If an add-on claims to have been
// installed longer ago than this the the test will fail.
const MAX_INSTALL_TIME = 10000;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

// install.rdf size, icon.png size
const ADDON1_SIZE = 705 + 16;

do_load_httpd_js();
var testserver;
var gInstallDate;

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager(1);
  // Make sure we only register once despite multiple calls
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.start(4444);

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Checks that an install from a local file proceeds as expected
function run_test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_install1"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "extension");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_true(install.addon.hasResource("install.rdf"));
    do_check_eq(install.addon.install, install);
    do_check_eq(install.addon.size, ADDON1_SIZE);

    let file = do_get_addon("test_install1");
    let uri = Services.io.newFileURI(file).spec;
    do_check_eq(install.addon.getResourceURI("install.rdf").spec, "jar:" + uri + "!/install.rdf");
    do_check_eq(install.addon.iconURL, "jar:" + uri + "!/icon.png");
    do_check_eq(install.iconURL, null);

    do_check_eq(install.sourceURI.spec, uri);
    do_check_eq(install.addon.sourceURI.spec, uri);

    AddonManager.getAllInstalls(function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      AddonManager.getInstallsByTypes(["foo"], function(fooInstalls) {
        do_check_eq(fooInstalls.length, 0);

        AddonManager.getInstallsByTypes(["extension"], function(extensionInstalls) {
          do_check_eq(extensionInstalls.length, 1);
          do_check_eq(extensionInstalls[0], install);

          prepare_test({
            "addon1@tests.mozilla.org": [
              "onInstalling"
            ]
          }, [
            "onInstallStarted",
            "onInstallEnded",
          ], check_test_1);
          install.install();
        });
      });
    });
  });
}

function check_test_1() {
  ensure_test_completed();
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(olda1) {
    do_check_eq(olda1, null);

    AddonManager.getAddonsWithOperationsByTypes(null, function(pendingAddons) {
      do_check_eq(pendingAddons.length, 1);
      do_check_eq(pendingAddons[0].id, "addon1@tests.mozilla.org");
      let iconFile = NetUtil.newURI(pendingAddons[0].iconURL)
                            .QueryInterface(AM_Ci.nsIFileURL).file;
      do_check_true(iconFile.exists());

      restartManager(1);

      AddonManager.getAllInstalls(function(activeInstalls) {
        do_check_eq(activeInstalls, 0);

        AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
          do_check_neq(a1, null);
          do_check_eq(a1.type, "extension");
          do_check_eq(a1.version, "1.0");
          do_check_eq(a1.name, "Test 1");
          do_check_true(isExtensionInAddonsList(profileDir, a1.id));
          do_check_true(do_get_addon("test_install1").exists());
          do_check_in_crash_annotation(a1.id, a1.version);
          do_check_eq(a1.size, ADDON1_SIZE);

          do_check_eq(a1.sourceURI.spec,
                      Services.io.newFileURI(do_get_addon("test_install1")).spec);
          // Should have been installed sometime in the last two second.
          let difference = Date.now() - a1.installDate.getTime();
          if (difference > MAX_INSTALL_TIME)
            do_throw("Add-on was installed " + difference + "ms ago");
          if (difference < 0)
            do_throw("Add-on was installed " + difference + "ms in the future");

          do_check_eq(a1.installDate.getTime(), a1.updateDate.getTime());

          do_check_true(a1.hasResource("install.rdf"));
          do_check_false(a1.hasResource("foo.bar"));

          let dir = profileDir.clone();
          dir.append("addon1@tests.mozilla.org");
          dir.append("install.rdf");
          let uri = Services.io.newFileURI(dir).spec;
          do_check_eq(a1.getResourceURI("install.rdf").spec, uri);

          a1.uninstall();
          restartManager(0);
          do_check_not_in_crash_annotation(a1.id, a1.version);

          run_test_2();
        });
      });
    });
  });
}

// Tests that an install from a url downloads.
function run_test_2() {
  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test 2");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
    do_check_eq(install.iconURL, null);
    do_check_eq(install.sourceURI.spec, url);

    AddonManager.getAllInstalls(function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_2);

      install.addListener({
        onDownloadProgress: function(install) {
          do_execute_soon(function() {
            Components.utils.forceGC();
          });
        }
      });

      install.install();
    });
  }, "application/x-xpinstall", null, "Test 2", null, "1.0");
}

function check_test_2(install) {
  ensure_test_completed();
  do_check_eq(install.version, "2.0");
  do_check_eq(install.name, "Real Test 2");
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_eq(install.addon.install, install);
  do_check_eq(install.iconURL, null);

  // Pause the install here and start it again in run_test_3
  do_execute_soon(function() { run_test_3(install); });
  return false;
}

// Tests that the downloaded XPI installs ok
function run_test_3(install) {
  prepare_test({
    "addon2@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_3);
  install.install();
}

function check_test_3() {
  ensure_test_completed();
  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(olda2) {
    do_check_eq(olda2, null);
    restartManager(1);

    AddonManager.getAllInstalls(function(installs) {
      do_check_eq(installs, 0);

      AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
        do_check_neq(a2, null);
        do_check_eq(a2.type, "extension");
        do_check_eq(a2.version, "2.0");
        do_check_eq(a2.name, "Real Test 2");
        do_check_true(isExtensionInAddonsList(profileDir, a2.id));
        do_check_true(do_get_addon("test_install2_1").exists());
        do_check_in_crash_annotation(a2.id, a2.version);
        do_check_eq(a2.sourceURI.spec,
                    "http://localhost:4444/addons/test_install2_1.xpi");

        // Should have been installed sometime in the last two second.
        let difference = Date.now() - a2.installDate.getTime();
        if (difference > MAX_INSTALL_TIME)
          do_throw("Add-on was installed " + difference + "ms ago");
        if (difference < 0)
          do_throw("Add-on was installed " + difference + "ms in the future");

        do_check_eq(a2.installDate.getTime(), a2.updateDate.getTime());
        gInstallDate = a2.installDate.getTime();

        run_test_4();
      });
    });
  });
}

// Tests that installing a new version of an existing add-on works
function run_test_4() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:4444/addons/test_install2_2.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "3.0");
    do_check_eq(install.name, "Test 3");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getAllInstalls(function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);
      do_check_eq(install.existingAddon, null);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_4);
      install.install();
    });
  }, "application/x-xpinstall", null, "Test 3", null, "3.0");
}

function check_test_4(install) {
  ensure_test_completed();

  do_check_eq(install.version, "3.0");
  do_check_eq(install.name, "Real Test 3");
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_neq(install.existingAddon);
  do_check_eq(install.existingAddon.id, "addon2@tests.mozilla.org");
  do_check_eq(install.addon.install, install);

  run_test_5();
  // Installation will continue when there is nothing returned.
}

// Continue installing the new version
function run_test_5() {
  prepare_test({
    "addon2@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_5);
}

function check_test_5(install) {
  ensure_test_completed();

  do_check_eq(install.existingAddon.pendingUpgrade.install, install);

  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(olda2) {
    do_check_neq(olda2, null);
    do_check_true(hasFlag(olda2.pendingOperations, AddonManager.PENDING_UPGRADE));

    AddonManager.getInstallsByTypes(null, function(installs) {
      do_check_eq(installs.length, 1);
      do_check_eq(installs[0].addon, olda2.pendingUpgrade);
      restartManager();

      AddonManager.getInstallsByTypes(null, function(installs) {
        do_check_eq(installs.length, 0);

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_neq(a2, null);
          do_check_eq(a2.type, "extension");
          do_check_eq(a2.version, "3.0");
          do_check_eq(a2.name, "Real Test 3");
          do_check_true(a2.isActive);
          do_check_true(isExtensionInAddonsList(profileDir, a2.id));
          do_check_true(do_get_addon("test_install2_2").exists());
          do_check_in_crash_annotation(a2.id, a2.version);
          do_check_eq(a2.sourceURI.spec,
                      "http://localhost:4444/addons/test_install2_2.xpi");

          do_check_eq(a2.installDate.getTime(), gInstallDate);
          // Update date should be later (or the same if this test is too fast)
          do_check_true(a2.installDate <= a2.updateDate);

          a2.uninstall();
          restartManager(0);

          run_test_6();
        });
      });
    });
  });
}

// Tests that an install that requires a compatibility update works
function run_test_6() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Real Test 4");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_6);
      install.install();
    });
  }, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
}

function check_test_6(install) {
  ensure_test_completed();
  do_check_eq(install.version, "1.0");
  do_check_eq(install.name, "Real Test 4");
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_eq(install.existingAddon, null);
  do_check_false(install.addon.appDisabled);
  run_test_7();
  return true;
}

// Continue the install
function run_test_7() {
  prepare_test({
    "addon3@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_7);
}

function check_test_7() {
  ensure_test_completed();
  AddonManager.getAddonByID("addon3@tests.mozilla.org", function(olda3) {
    do_check_eq(olda3, null);
    restartManager();

    AddonManager.getAllInstalls(function(installs) {
      do_check_eq(installs, 0);

      AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
        do_check_neq(a3, null);
        do_check_eq(a3.type, "extension");
        do_check_eq(a3.version, "1.0");
        do_check_eq(a3.name, "Real Test 4");
        do_check_true(a3.isActive);
        do_check_false(a3.appDisabled);
        do_check_true(isExtensionInAddonsList(profileDir, a3.id));
        do_check_true(do_get_addon("test_install3").exists());
        a3.uninstall();
        restartManager(0);

        run_test_8();
      });
    });
  });
}

function run_test_8() {
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);

  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_install3"), function(install) {
    do_check_true(install.addon.isCompatible);

    prepare_test({
      "addon3@tests.mozilla.org": [
        "onInstalling"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_8);
    install.install();
  });
}

function check_test_8() {
  restartManager(1);

  AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
    do_check_neq(a3, null);
    do_check_eq(a3.type, "extension");
    do_check_eq(a3.version, "1.0");
    do_check_eq(a3.name, "Real Test 4");
    do_check_true(a3.isActive);
    do_check_false(a3.appDisabled);
    do_check_true(isExtensionInAddonsList(profileDir, a3.id));
    do_check_true(do_get_addon("test_install3").exists());
    a3.uninstall();
    restartManager(0);

    run_test_9();
  });
}

// Test that after cancelling a download it is removed from the active installs
function run_test_9() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Real Test 4");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_9);
      install.install();
    });
  }, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
}

function check_test_9(install) {
  prepare_test({}, [
    "onDownloadCancelled"
  ]);

  install.cancel();

  ensure_test_completed();

  AddonManager.getAllInstalls(function(activeInstalls) {
    do_check_eq(activeInstalls.length, 0);

    run_test_10();
  });
}

// Tests that after cancelling a pending install it is removed from the active
// installs
function run_test_10() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Real Test 4");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({
        "addon3@tests.mozilla.org": [
          "onInstalling"
        ]
      }, [
        "onDownloadStarted",
        "onDownloadEnded",
        "onInstallStarted",
        "onInstallEnded"
      ], check_test_10);
      install.install();
    });
  }, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
}

function check_test_10(install) {
  prepare_test({}, [
    "onInstallCancelled"
  ]);

  install.cancel();

  ensure_test_completed();

  AddonManager.getAllInstalls(function(activeInstalls) {
    do_check_eq(activeInstalls.length, 0);

    end_test();
  });
}
