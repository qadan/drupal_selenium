<?php
/**
 * @file
 * Utilities and stuff.
 */

/**
 * Creates a message to display on the Selenium host testing config section.
 *
 * @param string $url
 *   The URL to check.
 *
 * @return string
 *   The message to display.
 */
function selenium_get_status_message($url) {
  $response = selenium_get_server_status($url);
  // If no response is received, the status returns FALSE.
  if (!$response) {
    $message = theme_image(array('path' => 'misc/watchdog-error.png', 'attributes' => array()));
    $message .= t("No response received from !selenium_url", array('!selenium_url' => $url));
    return $message;
  }

  // If NULL is returned, the server did not respond with well-formed JSON.
  if ($response === NULL) {
    $message = theme_image(array('path' => 'misc/watchdog-error.png', 'attributes' => array()));
    $message .= t("Response from !selenium_url was not well-formed JSON.", array('!selenium_url' => $url));
    return $message;
  }

  // Otherwise, we're good to check for a status.
  $status = $response['status'];
  $version = $response['value']['build']['version'];

  // If the status wasn't 0, we have a problem.
  if ($status !== 0) {
    module_load_include('inc', 'selenium', 'includes/selenium_web_driver');
    $status_codes = SeleniumWebDriver::getStatusCodes();
    $message = theme_image(array('path' => 'misc/watchdog-error.png', 'attributes' => array()));
    if (isset($status_codes[$status])) {
      $message .= t("Error !error_number encountered while trying to access the Selenium server !version at !selenium_url: !error", array(
        '!error_number' => $status,
        '!version' => $version,
        '!selenium_url' => $url,
        '!error' => $status_codes[$status],
      ));
      return $message;
    }
    // In case something ridiculous happened.
    else {
      $message .= t("Unknown error encountered while trying to access the Selenium server.");
      return $message;
    }
  }

  // Otherwise, we have smooth sailing.
  $message = theme_image(array('path' => 'misc/watchdog-ok.png', 'attributes' => array()));
  $message .= t('Successfully connected to Selenium server (version !version).', array(
    '!version' => $version));
  return $message;
}

/**
 * Returns the status of a Selenium server.
 *
 * @param string $url
 *   The URL of the server.
 *
 * @return bool|mixed
 *   Returns one of the following:
 *   FALSE if cURL failed to elicit a response from the server.
 *   NULL if a response was received but json_decode() failed to recognize it as
 *   properly-formed JSON.
 *   An array of decoded JSON if json_decode() recognized the response as well-
 *   formed JSON.
 */
function selenium_get_server_status($url) {
  // This is the path to check the server status.
  $url .= "/wd/hub/status";
  $curl = curl_init($url);
  curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
  curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-type: text/plain', 'Accept: application/json'));
  $response = curl_exec($curl);

  // If no response, return FALSE.
  if (!$response) {
    return FALSE;
  }

  // Otherwise, return the results from json_decode().
  return json_decode($response, TRUE);
}

/**
 * Checks to see if a browser executable is in $PATH and can be executed.
 *
 * @param string $browser
 *   The name of the executable to check for.
 *
 * @return bool
 *   Whether or not the given browser can be executed.
 */
function selenium_check_browser_status($browser) {
  if ($browser == 'chrome') {
    $path = exec("which google-chrome");
  }
  else {
    $path = exec("which $browser");
  }
  return is_executable($path) && in_array($browser, explode(', ', variable_get('selenium_allowed_browsers', 'firefox, chrome')));
}
