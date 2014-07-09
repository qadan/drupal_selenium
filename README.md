# Drupal Selenium Integration [![Build Status](https://travis-ci.org/discoverygarden/drupal_selenium.png?branch=7.x)](https://travis-ci.org/discoverygarden/drupal_selenium)

## Introduction

The Drupal Selenium Integration module is a heavily modified version of the possibly-defunct Drupal [Selenium project](http://drupal.org/project/selenium).

Besides restored functionality and many added features, it also includes several modifications to naturalize the Selenium module to Drupal, and to better integrate it with SimpleTest-style testing.

## Requirements

The Drupal Selenium Integration module requires several pieces of software to be installed on your computer before testing can begin:

* [Selenium server](http://seleniumhq.org/download/)
* One or more web browsers (Firefox and Chrome are currently supported).
* A display buffer of some kind for the browsers to work from.

## Installation

### Step 1)

Install the module using [standard Drupal methods](https://www.drupal.org/documentation/install/modules-themes/modules-7)

### Step 1.5)

Optionally, copy D7-core-selenium.patch, found in the module's root folder, to the Drupal root folder, and run the patch:

```bash
patch -p1 < D7-core-selenium.patch
```

This may be required for slower systems that take more time for the browser window to render.

### Step 2)

Download and run the Selenium server .jar:

```bash
curl "url_to_the_selenium_jar_download" > selenium.jar
java -jar selenium.jar # You should probably redirect this (to /dev/null, for example), e.g. by tacking >& /dev/null & at the end.
```

This will start a Selenium server on port 4444.

### Alternate Step 2)

For systems that have no natural display buffer, a virtual display buffer , such as X Virtual Frame Buffer (XVFB), may be required to start browsers from.

To install XVFB on Debian-based systems:

```bash
sudo apt-get -y install xvfb
curl "url_to_the_selenium_jar_download" > selenium.jar"
DISPLAY=:1 xvfb-run java -jar ~/selenium.jar # Similarly, this should probably be redirected.
```

This will also start a Selenium server on port 4444.

## Configuration

The Drupal Selenium Integration module can be configured on your site at admin/config/development/testing/settings.

The Selenium server host and port section allows you to specify where the Selenium server is hosted at. The page will attempt to establish a connection to that server, obtain its status, and let you know if any issues were encountered.

Allowed Selenium Browsers is a list of comma-and-space-separated browsers that tests consider to be valid for spinning up. The browsers 'firefox' and 'chrome' are handled especially by the module, and any others are handled by a generic driver that attempts to instantiate a Selenium session using it. THIS FEATURE SHOULD BE USED AT YOUR OWN RISK.
