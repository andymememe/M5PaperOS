#include "App/Launcher.h"
#include "App/Reader.h"
#include "App/Environment.h"
#include "App/Settings.h"
#include "App/Test.h"
#include "SystemManager.h"

LauncherApp appLauncher;
ReaderApp appReader;
EnvironmentApp appEnvironment;
TestApp appTest;
SettingsApp appSettings;

void setup() {
  Serial.begin(9600);
  sys.begin();

  appLauncher.registerApp(&appReader);
  appLauncher.registerApp(&appEnvironment);
  appLauncher.registerApp(&appTest);
  appLauncher.registerApp(&appSettings);

  sys.setLauncher(&appLauncher);
  sys.goHome();
}

void loop() { sys.run(); }