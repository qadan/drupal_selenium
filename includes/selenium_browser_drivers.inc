<?php
/**
 * @file
 * Web driver extensions for specific browsers.
 */

require_once drupal_get_path('module', 'selenium') . '/includes/selenium_web_driver.inc';

/**
 * Class of the connection to Firefox.
 */
class SeleniumFirefoxDriver extends SeleniumWebDriver {

  /**
   * Creates a SeleniumWebDriver customized for Firefox.
   *
   * @param string $user_agent
   *   The user agent to generate the driver with.
   * @param string $test_id
   *   The test ID used by the driver.
   *
   * @throws Exception
   *   If the session can't be generated.
   */
  public function __construct($user_agent, $test_id) {
    // Generate path info for the Firefox profile.
    $temporary_path = file_directory_temp();
    file_prepare_directory($temporary_path);
    $zip_file_path = $temporary_path . '/' . $test_id . '_firefox_profile.zip';

    // Generate the Firefox profile itself.
    $zip = new ZipArchive();
    $res = $zip->open($zip_file_path, ZipArchive::CREATE);
    if ($res === TRUE) {
      $zip->addFromString('prefs.js', 'user_pref("general.useragent.override", "' . $user_agent . '");');
      $zip->close();
    }
    else {
      throw new Exception('Can\'t create firefox profile ' . $zip_file_path);
    }

    // The Webdriver specs say we should encode the Firefox profile zip archive
    // in base64.
    $firefox_profile = base64_encode(file_get_contents($zip_file_path));

    // Start the browser with an appropriate set of capabilities.
    $capabilities = array(
      'browserName' => 'firefox',
      'javascriptEnabled' => TRUE,
      'platform' => 'ANY',
      'firefox_profile' => $firefox_profile,
    );
    $variables = array("desiredCapabilities" => $capabilities);
    $response = $this->execute("POST", "/session", $variables);

    // Store the session response, and parse out the session ID for storage as
    // well.
    $json = json_decode($response['body'], TRUE);
    $this->sessionResponse = $json;
    if (isset($json['sessionId'])) {
      $this->sessionId = $json['sessionId'];
    }
    // If we didn't get any of that, we need to generate an exception.
    else {
      $message = "Did not get a session id from " . SELENIUM_SERVER_URL . "\n";
      if (!empty($response['body'])) {
        $message .= "BODY: {$response['body']}\n";
      }
      if (!empty($response['header'])) {
        $message .= "HEADER: {$response['header']}\n";
      }
      if (empty($response)) {
        $message .= "No response from server.";
      }
      throw new Exception($message);
    }
  }
}

/**
 * Class of the connection to Chrome.
 */
class SeleniumChromeDriver extends SeleniumWebDriver {

  /**
   * Creates a SeleniumWebDriver customized for Chrome.
   *
   * @param string $user_agent
   *   The user agent to generate the driver with.
   *
   * @throws Exception
   *   If the session can't be generated.
   */
  public function __construct($user_agent) {
    // Start the browser with an appropriate set of capabilities.
    $capabilities = array(
      'browserName' => 'chrome',
      'javascriptEnabled' => TRUE,
      'platform' => 'ANY',
    );
    $args = explode(', ', variable_get('selenium_chrome_options', "start-maximized, window-size=1024,768"));
    $capabilities['chromeOptions']['args'] = array_merge(array("user-agent=$user_agent"), $args);
    $variables = array("desiredCapabilities" => $capabilities);
    $response = $this->execute("POST", "/session", $variables);

    // Store the session response.
    if (isset($response['body'])) {
      $this->sessionResponse = json_decode($response['body'], TRUE);
    }

    // Store the session response, and parse out the session ID for storage as
    // well.
    if (isset($response['body'])) {
      $this->sessionResponse = json_decode($response['body'], TRUE);
      if (isset($this->sessionResponse['sessionId'])) {
        $this->sessionId = $this->sessionResponse['sessionId'];
      }
    }
    // If we didn't get any of that, we need to generate an exception.
    else {
      $message = "Did not get a session id from " . SELENIUM_SERVER_URL . "\n";
      if (!empty($response['body'])) {
        $message .= "BODY: {$response['body']}\n";
      }
      if (!empty($response['header'])) {
        $message .= "HEADER: {$response['header']}\n";
      }
      if (empty($response)) {
        $message .= "No response from server.";
      }
      throw new Exception($message);
    }
  }
}

/**
 * Class of the connection to a generic browser driver.
 */
class SeleniumGenericDriver extends SeleniumWebDriver {

  /**
   * Creates a SeleniumWebDriver using generic settings.
   *
   * This is known to work with Opera and IExplorer. With anything else, use at
   * your own risk.
   *
   * @param string $name
   *   The name of the browser to generate a WebDriver for.
   *
   * @throws Exception
   *   If the session can't be generated.
   */
  public function __construct($name) {
    // Start the driver with a generic set of capabilities.
    $capabilities = array(
      'browserName' => $name,
      'javascriptEnabled' => TRUE,
      'platform' => 'ANY',
    );
    $variables = array("desiredCapabilities" => $capabilities);
    $response = $this->execute("POST", "/session", $variables);

    // Store the session response.
    if (isset($response['body'])) {
      $this->sessionResponse = json_decode($response['body'], TRUE);
      $this->sessionId = $this->sessionResponse['sessionId'];
    }

    // If we didn't get any of that, we need to generate an exception.
    else {
      $message = "Did not get a session id from " . SELENIUM_SERVER_URL . "\n";
      if (!empty($response['body'])) {
        $message .= "BODY: {$response['body']}\n";
      }
      if (!empty($response['header'])) {
        $message .= "HEADER: {$response['header']}\n";
      }
      if (empty($response)) {
        $message .= "No response from server.";
      }
      throw new Exception($message);
    }
  }
}
