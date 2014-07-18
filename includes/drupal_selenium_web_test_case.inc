<?php

/**
 * @file
 * Extension of the Drupal web test case that interacts with Selenium.
 */

// Server details constant.
define('SELENIUM_SERVER_URL', 'http://' . variable_get('selenium_server_host', 'localhost:4444') . "/wd/hub");

/**
 * Test case for Selenium.
 *
 * Refer to https://code.google.com/p/selenium/wiki/JsonWireProtocol for details
 * on the WebDriver wire protocol for information about what the functions in
 * this class do.
 */
class DrupalSeleniumWebTestCase extends DrupalWebTestCase {

  /**
   * An instance of SeleniumWebDriver representing the current session.
   *
   * @var SeleniumWebDriver
   */
  protected $driver;

  /**
   * The current browser used with the session.
   */
  protected $browser;

  /**
   * Forces the test to run on the actual site rather than a SimpleTest sandbox.
   *
   * This should be used with extreme caution.
   */
  public $onOriginal = FALSE;

  /**
   * Database prefix for the SimpleTest sandbox.
   */
  protected $sandboxDatabasePrefix;

  /**
   * Database prefix for the original site.
   */
  protected $originalDatabasePrefix;

  /**
   * Outputs verbose test results for some driver functions.
   *
   * @var bool
   */
  protected $verbose = FALSE;

  /**
   * The capture type set for verbose logging.
   *
   * @var string
   */
  protected $captureType = 'none';

  /**
   * Sets up the test.
   *
   * @see DrupalWebTestCase::setUp()
   */
  protected function setUp() {
    // Set what type of verbose capture method to use, if any.
    if (variable_get('simpletest_verbose', TRUE) == TRUE) {
      $this->captureType = variable_get('selenium_capture_method', 'html');
    }

    // Set allowed browsers before we set up the test.
    $allowed_browsers = variable_get('selenium_allowed_browsers', 'firefox, chrome');
    $allowed_browsers = explode(', ', $allowed_browsers);

    // Use the parent setUp() function to enable any required modules.
    $modules = func_get_args();
    if (isset($modules[0]) && is_array($modules[0])) {
      $modules = $modules[0];
    }

    // Always use this module.
    $modules[] = 'selenium';
    parent::setUp($modules);

    // Determine what browser to run. Also determine if we are allowed to use
    // the browser in question. Firefox is run when no other browser is present.
    $browser = 'firefox';
    if (in_array($this->browser, $allowed_browsers)) {
      $browser = $this->browser;
    }

    // Instantiate the driver.
    $this->driver = $this->seleniumDriver($browser);

    // Determine whether to run in the sandbox or on the original site.
    if ($this->onOriginal) {
      // Make the database switch.
      $this->sandboxDatabasePrefix = $this->databasePrefix;
      $this->originalDatabasePrefix = NULL;
      // Backup the sandbox connection.
      Database::renameConnection('default', 'simpletest_sandbox');
      $connection_info = Database::getConnectionInfo('simpletest_sandbox');
      Database::addConnectionInfo('default', 'default', $connection_info['default']);
      // Switch connection to the original site.
      $this->switchConnectionTo('simpletest_original_default');
    }
  }

  /**
   * Tears down the test.
   *
   * @see DrupalWebTestCase::tearDown()
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId
   */
  public function tearDown() {
    // If this was not run on the sandbox, switch back before completion.
    if ($this->onOriginal) {
      $this->switchConnectionTo('simpletest_sandbox');
    }
    parent::tearDown();
  }

  /**
   * Switches the database connection to the given.
   *
   * @param string $connection_name
   *   The connection to switch to.
   */
  protected function switchConnectionTo($connection_name = 'simpletest_original_default') {
    if ($this->checkSameConnection($connection_name)) {
      return;
    }
    switch ($connection_name) {
      case 'simpletest_sandbox':
        $this->databasePrefix = $this->sandboxDatabasePrefix;
        break;

      case 'simpletest_original_default':
      default:
        $this->databasePrefix = $this->originalDatabasePrefix;
        break;
    }
    // Remove default.
    Database::removeConnection('default');
    // Add default as a copy of the connection we need to switch to.
    $connection_info = Database::getConnectionInfo($connection_name);
    Database::addConnectionInfo('default', 'default', $connection_info['default']);
  }

  /**
   * Implements some logic to ensure we don't switch to the same connection.
   *
   * @param string $connection_name
   *   The connection name to check for.
   *
   * @return bool
   *   Whether or not this is the same connection.
   */
  protected function checkSameConnection($connection_name = 'simpletest_original_default') {
    switch ($connection_name) {
      case 'simpletest_sandbox':
        $same = ($this->databasePrefix === $this->sandboxDatabasePrefix);
        break;

      case 'simpletest_original_default':
      default:
        $same = ($this->databasePrefix === $this->originalDatabasePrefix);
        break;

    }
    if ($same) {
      $this->fail("Already on $connection_name connection", 'Connection');
    }
    return $same;
  }

  /**
   * Runs all tests in this class.
   *
   * @see DrupalWebTestCase::run()
   */
  public function run(array $methods = array()) {
    // Determine the status of the server.
    module_load_include('inc', 'selenium', 'includes/utilities');
    $url = variable_get('selenium_server_host', 'localhost:4444');
    $response = selenium_get_server_status($url);
    if (!$response || $response === NULL) {
      $caller = array(
        'file' => 'drupal_selenium_web_test_case.inc',
        'line' => __LINE__ - 4,
        'function' => 'DrupalSeleniumWebTestCase->run()',
      );
      DrupalTestCase::insertAssert($this->testId, get_class($this), FALSE, t("Unable to complete Selenium tests as a connection to the server could not be established.", 'Completion check', $caller));
      return;
    }
    elseif ($response['status'] !== 0) {
      $status_codes = SeleniumWebDriver::$statusCodes;
      $caller = array(
        'file' => 'drupal_selenium_web_test_case.inc',
        'line' => __LINE__ - 13,
        'function' => 'DrupalSeleniumWebTestCase->run()',
      );
      DrupalTestCase::insertAssert($this->testId, get_class($this), FALSE, t("Unable to complete Selenium tests as the server returned a non-zero status of !status (!message).", array(
        '!status' => $response['status'],
        '!message' => $status_codes[$response['status']],
      )), 'Completion check', $caller);
      return;
    }

    // Initialize verbose debugging.
    $this->verbose = variable_get('selenium_verbose_logging', FALSE);
    simpletest_verbose(NULL, variable_get('file_public_path', conf_path() . '/files'), get_class($this));

    // Grab the HTTP auth settings to be used when SimpleTest sends requests to
    // the site.
    $this->httpauth_method = variable_get('simpletest_httpauth_method', CURLAUTH_BASIC);
    $username = variable_get('simpletest_httpauth_username', NULL);
    $password = variable_get('simpletest_httpauth_password', NULL);
    if ($username && $password) {
      $this->httpauth_credentials = $username . ':' . $password;
    }

    // Start the error handler, then run through the tests.
    set_error_handler(array($this, 'errorHandler'));
    $class = get_class($this);
    // Iterate through all the methods in this class, unless a specific list of
    // methods to run was passed.
    $class_methods = get_class_methods($class);
    if ($methods) {
      $class_methods = array_intersect($class_methods, $methods);
    }
    foreach ($class_methods as $method) {
      // If the current method starts with "test", run it - it's a test.
      if (strtolower(substr($method, 0, 4)) == 'test') {
        // Get information about test case.
        $info = $this->getInfo();
        // If no browsers are set, we use internal.
        if (!isset($info['browsers'])) {
          $info['browsers'] = array('internal');
        }

        // Insert a fail record. This will be deleted on completion to ensure
        // that testing completed.
        $method_info = new ReflectionMethod($class, $method);
        $caller = array(
          'file' => $method_info->getFileName(),
          'line' => $method_info->getStartLine(),
          'function' => $class . '->' . $method . '()',
        );

        // Run test in each browser.
        module_load_include('inc', 'selenium', 'includes/utilities');
        foreach ($info['browsers'] as $browser) {
          // Check that the browser specified is available. Don't run if it's
          // not.
          $check = selenium_check_browser_status($browser);
          if (!$check) {
            DrupalTestCase::insertAssert($this->testId, $class, FALSE, t("Could not complete Selenium test using !browser; the browser was not found in \$PATH.", array('!browser' => $browser)), 'Browser check', $caller);
          }
          else {
            // If it is, set the browser, and start testing.
            $this->browser = $browser;

            $completion_check_id = DrupalTestCase::insertAssert($this->testId, $class, FALSE, t('The test did not complete due to a fatal error.'), 'Completion check', $caller);
            try {
              $this->setUp();
              // After the setUp() phase initializes the appropriate web driver,
              // the response from session initialization is stored. We check
              // it to see if the session even exists properly.
              if (isset($this->driver->sessionResponse['status']) && $this->driver->sessionResponse['status'] !== 0) {
                $message = $this->driver->sessionResponse['value']['message'];
                DrupalTestCase::insertAssert($this->testId, $class, FALSE, t("Invalid session response returned from Selenium server: !message", array('!message' => $message)), 'Server check', $caller);
              }
              // Otherwise, run the test.
              else {
                $this->$method();
              }
              // Finish up.
            }
            catch (Exception $e) {
              $this->exceptionHandler($e);
            }
            $this->tearDown();
            // Remove the completion check record.
            DrupalTestCase::deleteAssert($completion_check_id);
          }
        }
      }
    }
    // Clear out the error messages and restore error handler.
    drupal_get_messages();
    restore_error_handler();
  }

  /**
   * Initialize the specified driver.
   *
   * @param string $browser
   *   The browser to initialize a driver for.
   *
   * @throws Exception
   *   If no prefix is found.
   *
   * @return SeleniumWebDriver
   *   The specified driver.
   */
  protected function seleniumDriver($browser) {
    // A user agent is defined for local tests, which manipulate the database.
    // For remote tests, it's not necessary.
    if (isset($this->databasePrefix)) {
      $test_id = $GLOBALS['drupal_test_info']['test_run_id'];
      if (preg_match('/simpletest\d+/', $test_id, $matches)) {
        $user_agent = drupal_generate_test_ua($matches[0]);
      }
      else {
        throw new Exception('Test is not ready to init connection to WebDriver (no database prefix)');
      }
    }
    else {
      $user_agent = '';
      $test_id = $this->testId;
    }

    // Load the appropriate driver.
    module_load_include('inc', 'selenium', 'includes/selenium_browser_drivers');
    switch ($browser) {
      case 'firefox':
        return new SeleniumFirefoxDriver($user_agent, $test_id);

      case 'chrome':
        return new SeleniumChromeDriver($user_agent);

      default:
        return new SeleniumGenericDriver($browser);
    }
  }

  /**
   * Log in a user using the interface.
   *
   * @param stdClass $account
   *   The user account to log in with.
   */
  protected function drupalLogin(stdClass $account) {
    // If someone is logged in, log them out first.
    if ($this->loggedInUser) {
      $this->drupalLogout();
    }

    $this->openUrl('user');

    $this->sendKeysToElement($this->getElement('css=#edit-name'), $account->name);
    $this->sendKeysToElement($this->getElement('css=#edit-pass'), $account->pass_raw);
    $this->submitElement($this->getElement('css=#edit-submit'));

    // If a "log out" link appears on the page, it is almost certainly because
    // the login was successful.
    $pass = $this->assertLink(t('Log out'), 0, t('User %name successfully logged in.', array('%name' => $account->name)), t('User login'));

    if ($pass) {
      $this->loggedInUser = $account;
    }
  }

  /**
   * Logs a user out.
   */
  protected function drupalLogout() {
    // Make a request to the logout page, and redirect to the user page, the
    // idea being if you were properly logged out you should be seeing a login
    // screen.

    $this->openUrl('user/logout');
    $this->openUrl('user');

    $pass = $this->assertField('name', t('Username field found.'), t('Logout'));

    $pass = $pass && $this->assertField('pass', t('Password field found.'), t('Logout'));

    if ($pass) {
      $this->loggedInUser = FALSE;
    }
  }

  /**
   * Appends a verbose message to the test result, depending on the type set.
   */
  protected function appendVerboseMessage() {
    switch ($this->captureType) {
      case 'none':
        break;

      case 'html':
        $this->verbosePageContents();
        break;

      case 'screenshot':
        $this->verboseScreenshot();
        break;
    }
  }

  /**
   * Takes a screenshot from current page.
   *
   * Saves it to the verbose directory and adds a verbose message.
   */
  protected function verboseScreenshot() {
    // Take screenshot of current page.
    $screenshot = FALSE;
    try {
      $screenshot = $this->getScreenshot();
    }
    catch (Exception $e) {
      $this->verbose(t('No support for screenshots in %driver', array('%driver' => get_class($this->driver))));
    }
    if ($screenshot) {
      // Prepare directory.
      $directory = $this->originalFileDirectory . '/simpletest/verbose/screenshots';
      $writable = file_prepare_directory($directory, FILE_CREATE_DIRECTORY);
      if ($writable) {
        $testname = $this->getTestName();
        // Trying to save screenshot to verbose directory.
        $file = file_unmanaged_save_data($screenshot, $this->originalFileDirectory . '/simpletest/verbose/screenshots/' . $testname . '.png', FILE_EXISTS_RENAME);

        // Adding verbose message with link to screenshot.
        $this->error(l(t('Screenshot created.'), $GLOBALS['base_url'] . '/' . $file, array('attributes' => array('target' => '_blank'))), 'User notice');
      }
    }
  }


  /**
   * Adds the page contents to the verbose test results.
   *
   * Saves it to the verbose directory and adds a verbose message.
   */
  protected function verbosePageContents() {
    $page_contents = $this->getSource();
    if ($page_contents) {
      $testname = $this->getTestName();
      $directory = $this->originalFileDirectory . "/simpletest/verbose";
      $writable = file_prepare_directory($directory, FILE_CREATE_DIRECTORY);
      if ($writable) {
        $file = file_unmanaged_save_data($page_contents, $this->originalFileDirectory . '/simpletest/verbose/' . $testname . '.html', FILE_EXISTS_RENAME);
      }
      $this->error(l(t('Verbose page content'), $GLOBALS['base_url'] . '/' . $file, array('attributes' => array('target' => '_blank'))), 'User notice');
    }
  }

  /**
   * Asserts text on the given page.
   *
   * @param string $text
   *   The text to search for.
   * @param string $group
   *   The group the results belong to.
   * @param bool $not_exists
   *   Whether or not the text should be on the page.
   * @param string $message
   *   The message to pass through to the test results.
   *
   * @return bool
   *   TRUE for pass, or FALSE on failure.
   */
  protected function assertTextHelper($text, $group, $not_exists, $message = '') {
    $this->plainTextContent = filter_xss($this->getBodyText(), array());

    // Remove all newlines.
    $this->plainTextContent = str_replace("\n", '', $this->plainTextContent);

    if (!$message) {
      $message = !$not_exists ? t('"@text" found', array('@text' => $text)) : t('"@text" not found', array('@text' => $text));
    }
    return $this->assert($not_exists == (strpos($this->plainTextContent, $text) === FALSE), $message, $group);
  }

  /**
   * Asserts the title of the page.
   *
   * @param string $title
   *   The expected title of the page.
   * @param string $message
   *   The message to pass through to the test results.
   * @param string $group
   *   The group the results brlong to.
   *
   * @return bool
   *   TRUE for pass, or FALSE on failure.
   */
  protected function assertTitle($title, $message = '', $group = 'Other') {
    $actual = $this->getPageTitle();
    if (!$message) {
      $message = t('Page title @actual is equal to @expected.', array(
        '@actual' => var_export($actual, TRUE),
        '@expected' => var_export($title, TRUE),
      ));
    }
    return $this->assertEqual($actual, $title, $message, $group);
  }

  /**
   * Asserts that a field exists with the given name or ID.
   *
   * @param string $field
   *   Name or id of field to assert.
   * @param string $message
   *   Message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertField($field, $message = '', $group = 'Other') {
    try {
      $element = $this->getElement("name=$field");
    }
    catch (Exception $e) {
      try {
        $element = $this->getElement("id=$field");
      }
      catch (Exception $e) {
        $element = FALSE;
      }
    }
    return $this->assertTrue(!empty($element), $message ? $message : t('Field %locator found', array('%locator' => $field)), $group);
  }

  /**
   * Asserts that no field exists with the given name or ID.
   *
   * @param string $field
   *   Name or id of field to assert.
   * @param string $message
   *   Message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertNoField($field, $message = '', $group = 'Other') {
    try {
      $element = $this->getElement("name=$field");
    }
    catch (Exception $e) {
      try {
        $element = $this->getElement("id=$field");
      }
      catch (Exception $e) {
        $element = FALSE;
      }
    }
    return $this->assertTrue(empty($element), $message ? $message : t('Field %locator not found', array('%locator' => $field)), $group);
  }

  /**
   * Asserts that a link exists on the page.
   *
   * @param string $label
   *   The label of the link to assert.
   * @param int $index
   *   Which link to assert, if there are multiple.
   * @param string $message
   *   The message to pass to the test results.
   * @param string $group
   *   The group that the test results belong to.
   *
   * @return bool
   *   TRUE on success, or FALSE on failure.
   */
  protected function assertLink($label, $index = 0, $message = '', $group = 'Other') {
    $links = $this->waitForElements('link=' . $label);
    $message = ($message ? $message : t('Link with label %label found.', array('%label' => $label)));
    return $this->assert(isset($links[$index]), $message, $group);
  }

  /**
   * Follows a link by its name.
   *
   * Will click the first link found with this link text by default, or a later
   * one if an index is given. Case insensitive, with normalized space.
   *
   * @param string $label
   *   The label of the link.
   * @param int $index
   *   Link position counting from zero.
   *
   * @return mixed
   *   Page on success, or FALSE on failure.
   */
  protected function clickLink($label, $index = 0) {
    // Assert that link exists.
    if (!$this->assertLink($label, $index)) {
      return;
    }

    // Get link elements.
    $links = $this->waitForElements('link=' . $label);

    $link_element = $links[$index];

    // Get current and target urls.
    $url_before = $this->getUrl();
    $url_target = $this->getElementAttributeValue($link_element, 'href');

    $this->assertTrue(isset($links[$index]), t('Clicked link %label (@url_target) from @url_before', array(
      '%label' => $label,
      '@url_target' => $url_target,
      '@url_before' => $url_before,
    )), t('Browser'));

    // Click on element;
    $this->clickElement($link_element);
  }

  /**
   * Asserts that no link exists on the page with the label specified.
   *
   * @param string $label
   *   The label of the link to search for.
   * @param int $index
   *   Link position counting from zero.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE if the assertion succeeded, FALSE otherwise.
   */
  protected function assertNoLink($label, $index = 0, $message = '', $group = 'Other') {
    $links = $this->waitForElements('link=' . $label);
    $message = ($message ? $message : t('Link with label %label not found.', array('%label' => $label)));
    return $this->assert(!isset($links[$index]), $message, $group);
  }

  /**
   * Asserts that a link containing a given href (part) is found.
   *
   * @param string $href
   *   The full or partial value of the 'href' attribute of the anchor tag.
   * @param int $index
   *   Link position counting from zero.
   * @param string $message
   *   The essage to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE if the assertion succeeded, FALSE otherwise.
   */
  protected function assertLinkByHref($href, $index = 0, $message = '', $group = 'Other') {
    $links = $this->getAllElements("//a[contains(@href, '$href')]");
    $message = ($message ? $message : t('Link containing href %href found.', array('%href' => $href)));
    return $this->assert(isset($links[$index]), $message, $group);
  }

  /**
   * Asserts that a link containing a given href (part) is not found.
   *
   * @param string $href
   *   The full or partial value of the 'href' attribute of the anchor tag.
   * @param int $index
   *   Link position counting from zero.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE if the assertion succeeded, FALSE otherwise.
   */
  protected function assertNoLinkByHref($href, $index = 0, $message = '', $group = 'Other') {
    $links = $this->getAllElements("//a[contains(@href, '$href')]");
    $message = ($message ? $message : t('Link containing href %href not found.', array('%href' => $href)));
    return $this->assert(!isset($links[$index]), $message, $group);
  }

  /**
   * Asserts that a select option in the current page is checked.
   *
   * @param string|SeleniumWebElement $locator
   *   The locator for the select element, or the actual SeleniumWebElement to
   *   use.
   * @param string $option
   *   The option to assert.
   * @param string $message
   *   The message to display.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   *
   * @todo $id is unusable.
   */
  protected function assertOptionSelected($locator, $option, $message = '') {
    $selected = FALSE;
    if (!is_object($locator)) {
      $element = $this->getElement($locator);
    }
    else {
      $element = $locator;
    }
    $is_select = $element && $this->getElementTagName($element) == 'select';
    if ($is_select) {
      $id = $this->getElementAttributeValue($element, 'id');
      $message = $message ? $message : t('Option @option for field @id is selected.', array('@option' => $option, '@id' => $id));
      $selected_options = $this->getSelectedElement($element);
      foreach ($selected_options as $selected_option) {
        if ($this->getElementText($selected_option) == $option) {
          $selected = TRUE;
          break;
        }
      }
    }
    else {
      $message = t('There is no element with locator @locator or element is not select list.', array('@locator' => $locator));
    }

    return $this->assertTrue($is_select && $selected, $message, t('Browser'));
  }

  /**
   * Asserts that the given option is not selected.
   *
   * @param string|SeleniumWebElement $locator
   *   The locator for the select element, or the actual SeleniumWebElement to
   *   use.
   * @param string $option
   *   The option to assert.
   * @param string $message
   *   The message to display.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertNoOptionSelected($locator, $option, $message = '') {
    $selected = FALSE;
    if (!is_object($locator)) {
      $element = $this->getElement($locator);
    }
    else {
      $element = $locator;
    }
    $is_select = $element && $this->getElementTagName($element) == 'select';
    if ($is_select) {
      $id = $this->getElementAttributeValue($element, 'id');
      $message = $message ? $message : t('Option @option for field @id is not selected.', array('@option' => $option, '@id' => $id));
      $selected_options = $this->getSelectedElement($element);
      foreach ($selected_options as $selected_option) {
        if ($this->getElementText($selected_option) == $option) {
          $selected = TRUE;
          break;
        }
      }
    }
    else {
      $message = t('There is no element with locator @locator or element is not select list.', array('@locator' => $locator));
    }

    return $this->assertTrue($is_select && !$selected, $message, t('Browser'));
  }

  /**
   * Asserts that a checkbox field in the current page is checked.
   *
   * @param string|SeleniumWebElement $locator
   *   The locator for the select element, or the actual SeleniumWebElement to
   *   use.
   * @param string $message
   *   The message to display.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertFieldChecked($locator, $message = '') {
    if (!is_object($locator)) {
      $element = $this->getElement($locator);
    }
    else {
      $element = $locator;
    }
    $is_checkbox = $element && ($this->getElementTagName($element) == 'checkbox' || $this->getElementAttributeValue($element, 'type') == 'checkbox');
    if ($is_checkbox) {
      $id = $this->getElementAttributeValue($element, 'id');
      $message = $message ? $message : t('Checkbox field @id is checked.', array('@id' => $id));
    }
    else {
      $message = t('There is no element with locator @locator or element is not checkbox.', array('@locator' => $locator));
    }

    return $this->assertTrue($is_checkbox && $this->isElementSelected($element), $message, t('Browser'));
  }

  /**
   * Asserts that a checkbox field in the current page is not checked.
   *
   * @param string|SeleniumWebElement $locator
   *   The locator for the select element, or the actual SeleniumWebElement to
   *   use.
   * @param string $message
   *   The message to display.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertNoFieldChecked($locator, $message = '') {
    if (!is_object($locator)) {
      $element = $this->getElement($locator);
    }
    else {
      $element = $locator;
    }
    $is_checkbox = $element && ($this->getElementTagName($element) == 'checkbox' || $this->getElementAttributeValue($element, 'type') == 'checkbox');
    if ($is_checkbox) {
      $id = $this->getElementAttributeValue($element, 'id');
      $message = $message ? $message : t('Checkbox field @id is not checked.', array('@id' => $id));
    }
    else {
      $message = t('There is no element with locator @locator or element is not checkbox.', array('@locator' => $locator));
    }

    return $this->assertTrue($is_checkbox && !$this->isElementSelected($element), $message, t('Browser'));
  }

  /**
   * Asserts that each HTML ID is used for just a single element.
   *
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   * @param array $ids_to_skip
   *   An optional array of IDs to skip when checking for duplicates. It is
   *   always a bug to have duplicate HTML IDs, so this parameter is to enable
   *   incremental fixing of core code. Whenever a test passes this parameter,
   *   it should add a "todo" comment above the call to this function explaining
   *   the legacy bug that the test wishes to ignore.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertNoDuplicateIds($message = '', $group = 'Other', $ids_to_skip = array()) {
    try {
      $elements = $this->getAllElements("//*[@id]");
      $status = TRUE;
      foreach ($elements as $element) {
        $id = (string) $this->getElementAttributeValue($element, 'id');
        if (isset($seen_ids[$id]) && !in_array($id, $ids_to_skip)) {
          $this->fail(t('The HTML ID %id is unique.', array('%id' => $id)), $group);
          $status = FALSE;
        }
        $seen_ids[$id] = TRUE;
      }
    }
    catch (Exception $e) {
      $status = FALSE;
    }
    return $this->assertTrue($status, $message ? $message : t('No Duplicate Ids'), $group);
  }

  /**
   * Asserts that a field exists in the page with the given name and value.
   *
   * @param string $name
   *   The name of the field to assert.
   * @param string $value
   *   The value of the field to assert.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertFieldByName($name, $value = '', $message = '', $group = 'Other') {
    try {
      $element = $this->getElement("name=$name");
      if ($value) {
        $element = $this->elementValue($element, $value);
      }
    }
    catch (Exception $e) {
      $element = FALSE;
    }
    return $this->assertTrue(!empty($element), $message ? $message : t('Field found by name'), $group);
  }

  /**
   * Asserts that no field exists in the page with the given name and value.
   *
   * @param string $name
   *   The name of the field to assert.
   * @param string $value
   *   The value of the field to assert.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertNoFieldByName($name, $value = '', $message = '', $group = 'Other') {
    try {
      $element = $this->getElement("name=$name");
      if ($value) {
        $element = $this->elementValue($element, $value);
      }
    }
    catch (Exception $e) {
      $element = FALSE;
    }
    return $this->assertTrue(empty($element), $message ? $message : t('Field found by name'), $group);
  }

  /**
   * Asserts that a field exists in the page with the given ID and value.
   *
   * @param string $id
   *   The ID of the field to assert.
   * @param string $value
   *   The value of the field to assert.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertFieldById($id, $value = '', $message = '', $group = 'Other') {
    try {
      $element = $this->getElement("id=$id");
      if ($value) {
        $element = $this->elementValue($element, $value);
      }
    }
    catch (Exception $e) {
      $element = FALSE;
    }
    return $this->assertTrue(!empty($element), $message ? $message : t('Field found by id'), $group);
  }

  /**
   * Asserts that no field exists in the page with the given ID and value.
   *
   * @param string $id
   *   The ID of the field to assert.
   * @param string $value
   *   The value of the field to assert.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertNoFieldById($id, $value = '', $message = '', $group = 'Other') {
    try {
      $element = $this->getElement("id=$id");
      if ($value) {
        $element = $this->elementValue($element, $value);
      }
    }
    catch (Exception $e) {
      $element = FALSE;
    }
    return $this->assertTrue(empty($element), $message ? $message : t('Field found by id'), $group);
  }

  /**
   * Checks that the value of a given element equals the given value.
   *
   * @param SeleniumWebElement $element
   *   The element to check the value of.
   * @param string $value
   *   The value to confirm exists on the element.
   *
   * @returns bool
   *   Whether or not the given value exists on the element.
   */
  protected function elementValue($element, $value) {
    $element_text = NULL;
    $element_value = NULL;
    if ($this->getElementTagName($element) == 'select') {
      $element_value = $this->getElementAttributeValue($this->getElementSelectedOption($element), 'value');
      $element_text = $this->getElementText($this->getElementSelectedOption($element));
    }
    else {
      $element_value = trim($this->getElementText($element));
    }
    return $value == $element_value || $value == $element_text;
  }

  /**
   * Asserts that a field exists in the current page by the given XPath.
   *
   * @param string $xpath
   *   XPath used to find the field.
   * @param string $value
   *   An optional value to assert on the field.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertFieldByXPath($xpath, $value = NULL, $message = '', $group = 'Other') {
    try {
      $element = $this->getElement($xpath);
      if ($value) {
        $element = $this->elementValue($element, $value);
      }
    }
    catch (Exception $e) {
      $element = FALSE;
    }
    return $this->assertTrue(!empty($element), $message ? $message : t('Field found by Xpath'), $group);
  }

  /**
   * Execute a POST request on a Drupal page.
   *
   * @param string $path
   *   Location of the post form. Either a Drupal path or an absolute path or
   *   NULL to post to the current page. For multi-stage forms you can set the
   *   path to NULL and have it post to the last received page. Example:   *
   * @code
   *   // First step in form.
   *   $edit = array(...);
   *   $this->formSubmit('some_url', $edit, t('Save'));
   *
   *   // Second step in form.
   *   $edit = array(...);
   *   $this->formSubmit(NULL, $edit, t('Save'));
   * @endcode
   * @param array $edit
   *   Field data in an associative array. Changes the current input fields
   *   (where possible) to the values indicated. A checkbox can be set to
   *   TRUE to be checked and FALSE to be unchecked. Note that when a form
   *   contains file upload fields, other fields cannot start with the '@'
   *   character.
   *
   *   Multiple select fields can be set using name[] and setting each of the
   *   possible values. Example:
   * @code
   *   $edit = array();
   *   $edit['name[]'] = array('value1', 'value2');
   * @endcode
   * @param string $submit
   *   Value of the submit button whose click is to be emulated. For example,
   *   t('Save'). The processing of the request depends on this value. For
   *   example, a form may have one button with the value t('Save') and another
   *   button with the value t('Delete'), and execute different code depending.
   * @param array $disable_js
   *   An array of JavaScript scripts to disable that would otherwise hide
   *   elements.
   *
   * @return bool
   *   TRUE on pass, FALSE on failure.
   */
  protected function postUrl($path, $edit, $submit, $disable_js = array()) {
    if ($this->getUrl() != $path && !is_null($path)) {
      $this->openUrl($path);
    }

    // Disable javascripts that hide elements.
    $this->disableJs($disable_js);

    // Find form elements and set the values.
    foreach ($edit as $selector => $value) {
      $element = $this->getElement("name=$selector");
      // Type of input element. Can be textarea, select or input. If input,
      // we need to check 'type' property.
      $type = $this->getElementTagName($element);
      if ($type == 'input') {
        $type = $this->getElementAttributeValue($element, 'type');
      }
      switch ($type) {
        case 'text':
        case 'textarea':
          // Clear element first then send text data.
          $this->clearElement($element);
          $this->sendKeysToElement($element, $value);
          break;

        case 'select':
          $this->selectElementOptionByValue($element, $value);
          break;

        case 'radio':
          $elements = $this->getAllElements("name=$selector");
          foreach ($elements as $element) {
            if ($this->getElementAttributeValue($element, 'value') == $value) {
              $this->clickElement($element);
            }
          }
          break;

        case 'checkbox':
          $elements = $this->getAllElements("name=$selector");
          if (!is_array($value)) {
            $value = array($value);
          }
          foreach ($elements as $element) {
            $element_value = $this->getElementAttributeValue($element, 'value');
            $element_selected = $this->isElementSelected($element);
            // Click on element if it should be selected but isn't or if element
            // shouldn't be selected but it is.
            if ((in_array($element_value, $value) && !$element_selected) ||
                (!in_array($element_value, $value) && $element_selected)) {
              $this->clickElement($element);
            }
          }
          break;
      }

    }

    // Find button and submit the form.
    $this->clickSubmitButton($submit);

    // Wait for the page to load.
    $this->waitForElements('css=body');
    $this->appendVerboseMessage();
  }

  /**
   * Clicks on a submit button with the given label.
   *
   * @param string $value
   *   The label of the submit button.
   *
   * @return bool
   *   TRUE if the button is found and clicked, or FALSE if it isn't.
   */
  public function clickSubmitButton($value) {
    $elements = $this->getAllElements('name=op');
    if (empty($elements)) {
      $this->fail('No submit button elements found on page.', 'Selenium WebDriver');
      return FALSE;
    }
    foreach ($elements as $element) {
      $val = $this->getElementValue($element);
      if ($val == $value) {
        $this->submitElement($element);
        return TRUE;
      }
    }
    $this->fail("No submit button element found on the page with label $value.", 'Selenium WebDriver');
    return FALSE;
  }

  /**
   * Injects javascript code to disable work of some of the drupal javascripts.
   *
   * For example, vertical tabs hide some elements on a form. This leads to
   * situations where Selenium can't access the hidden fields. So, if we used
   * the postUrl method with these hidden elements, we would not be able to
   * submit the form properly.
   *
   * @param array $scripts
   *   The scripts to disable.
   */
  public function disableJs($scripts) {
    $scripts += array(
      'vertical tabs' => TRUE,
    );

    foreach ($scripts as $type => $execute) {
      if (!$execute) {
        continue;
      }
      $javascript = '';
      switch ($type) {
        case 'vertical tabs':
          $javascript = 'jQuery(".vertical-tabs-pane").show();';
          break;
      }
      // Inject javascript.
      if (!empty($javascript)) {
        $this->executeJsSync($javascript);
      }
    }
  }

  /**
   * Get name of current test running.
   *
   * @return string
   *   The name of the current test.
   */
  protected function getTestName() {
    $backtrace = debug_backtrace();
    foreach ($backtrace as $bt_item) {
      if (strtolower(substr($bt_item['function'], 0, 4)) == 'test') {
        return $bt_item['function'];
      }
    }
    return '';
  }

  /**
   * Get the selected value from a select field.
   *
   * @param SeleniumWebElement $element
   *   The element to get the selected field of.
   *
   * @return array
   *   The selected options array.
   */
  protected function getSelectedElement(SeleniumWebElement $element) {
    $result = array();
    foreach ($this->getOptions($element) as $option) {
      if ($this->isElementSelected($option)) {
        $result[] = $option;
      }
    }
    return $result;
  }

  /**
   * Asserts that the browser's URL matches the given path.
   *
   * @param string $path
   *   The expected path.
   * @param array $options
   *   (optional) Any additional options to pass for $path to url().
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertUrl($path, array $options = array(), $message = '', $group = 'Other') {
    if (!$message) {
      $message = t('Current URL is @url.', array(
        '@url' => var_export(url($path, $options), TRUE),
      ));
    }
    $options['absolute'] = TRUE;
    return $this->assertEqual($this->getUrl(), url($path, $options), $message, $group);
  }

  /**
   * Pass if the page title is not the given string.
   *
   * @param string $title
   *   The string the title should not be.
   * @param string $message
   *   The message to display.
   * @param string $group
   *   The group this message belongs to.
   *
   * @return bool
   *   TRUE on pass, FALSE on fail.
   */
  protected function assertNoTitle($title, $message = '', $group = 'Other') {
    if (!$message) {
      $message = t('Page title @actual is not equal to @unexpected.', array(
        '@actual' => var_export($this->getPageTitle(), TRUE),
        '@unexpected' => var_export($title, TRUE),
      ));
    }
    return $this->assertNotEqual($this->getPageTitle(), $title, $message, $group);
  }

  /**
   * WebDriver functions for getting page/server data.
   */

  /**
   * The getters are preeeeeety much all the same, so this shortens them up.
   *
   * @param string $path
   *   The path suffix to use when making the execute request.
   * @param string $message
   *   A message to pass through to the results.
   * @param string $method
   *   The HTTP method used, in case it's different; defaults to GET.
   * @param null|array $variables
   *   Variables to be passed to the executor; defaults to NULL for none.
   * @param null|string $attribute
   *   An attribute to select from the JSON value array.
   *
   * @returns array|bool
   *   The value array from the response, or FALSE on failure.
   */
  protected function getterHelper($path, $message, $method = 'GET', $variables = NULL, $attribute = NULL) {
    try {
      $response = $this->driver->execute($method, $path, $variables);
      $result = $this->driver->getJSONValue($response, $attribute);
    }
    catch (Exception $e) {
      $this->fail("$message: $e", "Selenium WebDriver");
      return FALSE;
    }
    if ($this->verbose) {
      $this->pass("$message.", "Selenium WebDriver");
    }
    return $result;
  }

  /**
   * Get the current URL of the browser.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/url
   *
   * @return array|bool
   *   The value array from the response, or FALSE on failure.
   */
  public function getUrl() {
    return $this->getterHelper('/session/:sessionId/url', 'Got the current URL');
  }

  /**
   * Get the current page title.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/title
   *
   * @return array|bool
   *   The value array from the response, or FALSE on failure.
   */
  public function getPageTitle() {
    return $this->getterHelper('/session/:sessionId/title', 'Got the current page title');
  }

  /**
   * Get the current page source.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/source
   *
   * @return array|bool
   *   The value array from the response, or FALSE on failure.
   */
  public function getSource() {
    return $this->getterHelper('/session/:sessionId/source', 'Got the current page source');
  }

  /**
   * Get the visible text in the body.
   *
   * @return string|bool
   *   The visible text in the body, or FALSE on failure.
   */
  public function getBodyText() {
    try {
      $result = $this->getElementText($this->getElement("tag name=body"));
    }
    catch (Exception $e) {
      $this->fail("Failed to get the text of the body: $e", "Selenium WebDriver");
      return FALSE;
    }
    $this->pass("Got the text of the body.", "Selenium WebDriver");
    return $result;
  }

  /**
   * Get a screenshot of the current page.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/screenshot
   *
   * @return string|bool
   *   The screenshot data as a string, or FALSE on failure.
   */
  public function getScreenshot() {
    return base64_decode($this->getterHelper('/session/:sessionId/screenshot', 'Took a screenshot of the page'));
  }

  /**
   * Get an element.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element
   *
   * @param string $locator
   *   The method for locating the element.
   *
   * @return SeleniumWebElement|bool
   *   The element object generated, or FALSE on failure.
   */
  public function getElement($locator) {
    $variables = $this->driver->parseLocator($locator);
    $element_id = $this->getterHelper('/session/:sessionId/element', "Got element from locator $locator", 'POST', $variables, 'ELEMENT');
    if ($element_id !== FALSE) {
      module_load_include('inc', 'selenium', 'includes/selenium_web_element');
      return new SeleniumWebElement($this->driver, $element_id, $locator);
    }
    else {
      return FALSE;
    }
  }

  /**
   * Wait for elements to be ready, and get them when they are.
   *
   * @param string $locator
   *   The method for locating the elements.
   * @param int $seconds
   *   The number of seconds to wait; defaults to 10.
   *
   * @return SeleniumWebElement[]|bool
   *   The elements found, or FALSE on failure.
   */
  public function waitForElements($locator, $seconds = 10) {
    $elements = NULL;
    while ($seconds > 0 && empty($elements)) {
      $elements = $this->getAllElements($locator);
      sleep(1);
      $seconds--;
    }
    if (empty($seconds)) {
      $this->fail("Failed to find any elements within $seconds seconds using locator $locator.", "Selenium WebDriver");
      return FALSE;
    }
    else {
      return $elements;
    }
  }

  /**
   * Wait for visible elements.
   *
   * Checks only the element specified by the index $item.
   *
   * @param string $locator
   *   The method for locating the elements.
   * @param int $item
   *   The index of the item to return.
   * @param int $seconds
   *   The number of seconds to check for; defaults to 10.
   *
   * @return SeleniumWebElement
   *   The element that became visible.
   */
  public function waitForVisibleElements($locator, $item = 0, $seconds = 10) {
    $elements = NULL;
    while ($seconds > 0) {
      $elements = $this->getAllElements($locator);
      if (!empty($elements) && isset($elements[$item])) {
        $element = $elements[$item];
        if ($this->isElementDisplayed($element)) {
          return $elements;
        }
      }
      sleep(1);
      $seconds--;
    }
    if (empty($seconds)) {
      $this->fail("Failed to find any visible elements within $seconds seconds using locator $locator.", "Selenium WebDriver");
      return FALSE;
    }
    else {
      return $elements;
    }
  }

  /**
   * Get all elements found via the locator.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/elements
   *
   * @param string $locator
   *   The method for locating the elements.
   *
   * @return SeleniumWebElement[]|bool
   *   All elements found via the locator method, or FALSE on failure.
   */
  public function getAllElements($locator) {
    $variables = $this->driver->parseLocator($locator);
    $element_ids = $this->getterHelper('/session/:sessionId/elements', "Found elements using locator $locator", 'POST', $variables, 'ELEMENT');
    if ($element_ids !== FALSE) {
      $elements = array();
      module_load_include('inc', 'selenium', 'includes/selenium_web_element');
      foreach ($element_ids as $element_id) {
        $elements[] = new SeleniumWebElement($this->driver, $element_id, $locator);
      }
      return $elements;
    }
    else {
      return FALSE;
    }
  }

  /**
   * Get the element that currently has focus.
   *
   * @return SeleniumWebElement|bool
   *   The element that currently has focus, or FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/active
   */
  public function getActiveElement() {
    $element_id = $this->getterHelper('/session/:sessionId/element/active', 'Got the active element', 'POST', NULL, 'ELEMENT');
    if ($element_id !== FALSE) {
      module_load_include('inc', 'selenium', 'includes/selenium_web_element');
      return new SeleniumWebElement($this->driver, $element_id, "active=true");
    }
    return FALSE;
  }

  /**
   * Retrive the current window handle.
   *
   * @return array
   *   The value array from the response.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/window_handle
   */
  public function getWindowHandle() {
    return $this->getterHelper('/session/:sessionId/window_handle', 'Got the current window handle');
  }

  /**
   * Retrieve a list of all window handles available to the session.
   *
   * @return array
   *   The value array from the response.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/window_handles
   */
  public function getAllWindowHandles() {
    return $this->getterHelper('/session/:sessionId/window_handles', 'Got all window handles');
  }

  /**
   * Get all cookies.
   *
   * @return array
   *   The value array from the response.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/cookie
   */
  public function getAllCookies() {
    return $this->getterHelper('/session/:sessionId/cookie', 'Got all cookies');
  }

  /**
   * Get a specific cookie.
   *
   * @param string $name
   *   Name of the cookie.
   * @param string $property
   *   Which cookie property to return. Set to NULL to retrieve all properties.
   *
   * @return array|string|bool
   *   An array of all properties of the specified cookie, or a string
   *   representing the selected property of the specified cookie. FALSE on
   *   failure.
   */
  public function getCookie($name, $property = NULL) {
    $all_cookies = $this->getAllCookies();
    if ($all_cookies !== FALSE) {
      foreach ($all_cookies as $cookie) {
        if ($cookie['name'] == $name) {
          if (is_null($property)) {
            return $cookie;
          }
          return $cookie[$property];
        }
      }
    }
    else {
      if (is_null($property)) {
        $property = 'all properties';
      }
      return $this->fail("Failed to get property: $property from cookie: $name.", "Selenium WebDriver");
    }
    return FALSE;
  }

  /**
   * Gets the text of the currently displayed JavaScript dialog.
   *
   * Finds dialogs generated via alert(), confirm(), or prompt().
   *
   * Throws an error if there is no current JS dialog.
   *
   * @return string
   *   The text of the currently displayed JavaScript dialog.
   */
  public function getJSDialogText() {
    return $this->getterHelper('/session/:sessionId/alert_text', "Got the text of the current JavaScript alert window");
  }

  /**
   * Setter functions.
   */

  /**
   * Helper for performing simple setter functions.
   *
   * @param string $path
   *   The relative URL to paste on the end of the request.
   * @param string $message
   *   The message to pass through at the end.
   * @param array|null $variables
   *   Any variables required by the request executor. Use NULL for none.
   * @param string $http_method
   *   The HTTP method to use; defaults to POST.
   *
   * @return bool
   *   TRUE on success, or FALSE on failure.
   */
  protected function setterHelper($path, $message, $variables = NULL, $http_method = 'POST') {
    try {
      $this->driver->execute($http_method, $path, $variables);
    }
    catch (Exception $e) {
      $this->fail("$message: $e", "Selenium WebDriver");
      return FALSE;
    }
    $this->pass("$message.", "Selenium WebDriver");
    return TRUE;
  }

  /**
   * Set the amount of time, in milliseconds, that asynchronous scripts execute.
   *
   * @param int $milliseconds
   *   The time to execute, in milliseconds.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/timeouts/async_script
   */
  public function setAsyncTimeout($milliseconds) {
    $variables = array("ms" => $milliseconds);
    return $this->setterHelper("/session/:sessionId/timeouts/async_script", "Set the asynchronous script timeout to $milliseconds milliseconds", $variables);
  }

  /**
   * Set the web driver's implicit wait time when searching for elements.
   *
   * @param int $milliseconds
   *   The time to execute, in milliseconds.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/timeouts/implicit_wait
   */
  public function setImplicitWait($milliseconds) {
    $variables = array('ms' => $milliseconds);
    return $this->setterHelper("/session/:sessionId/timeouts/implicit_wait", "Set the implicit wait time to $milliseconds milliseconds", $variables);
  }

  /**
   * Navigates to the given URL.
   *
   * @param string $url
   *   The URL to navigate to.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/url
   */
  public function openUrl($url) {
    if (is_array($url)) {
      $path = $url[0];
      $options = $url[1];
      $options['absolute'] = TRUE;
      $full_url = url($path, $options);
    }
    else {
      $full_url = url($url, array('absolute' => TRUE));
    }
    $variables = array("url" => $full_url);
    $response = $this->setterHelper('/session/:sessionId/url', "Opened URL: $url", $variables);
    $this->waitForElements('css=body');
    $this->appendVerboseMessage();
    return $response;
  }

  /**
   * Navigate forward in browser's history.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/forward
   */
  public function historyForward() {
    return $this->setterHelper('/session/:sessionId/forward', "Moved forward in the browser history");
  }

  /**
   * Navigate back in browser's history.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/back
   */
  public function historyBack() {
    return $this->setterHelper('/session/:sessionId/back', "Moved back in the browser history");
  }

  /**
   * Refresh the page.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/refresh
   */
  public function refresh() {
    return $this->setterHelper('/session/:sessionId/refresh', "Refreshed the active page");
  }

  /**
   * Change focus to another opened window.
   *
   * @param string $window_title
   *   The title of the window to change to.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/window
   */
  public function selectWindow($window_title) {
    $variables = array('name' => $window_title);
    return $this->setterHelper('/session/:sessionId/window', "Changed focus to window $window_title", $variables);
  }

  /**
   * Close the current window.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/window
   */
  public function closeWindow() {
    $handle = $this->getWindowHandle();
    return $this->setterHelper('/session/:sessionId/window', "Closed window $handle", NULL, 'DELETE');
  }

  /**
   * Change focus to another frame on the page.
   *
   * @param string|int|null $identifier
   *   The frame's identifier, or NULL for the default content.
   *
   * @return bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/frame
   */
  public function selectFrame($identifier) {
    $variables = array("id" => $identifier);
    return $this->setterHelper('/session/:sessionId/frame', "Changed focus to frame $identifier", $variables);
  }

  /**
   * Change focus to the parent frame.
   *
   * If there is no parent frame, the focus will remain unchanged.
   *
   * @return bool
   *   TRUE on pass, FALSE on failure.
   */
  public function selectParentFrame() {
    return $this->setterHelper('/session/:sessionId/frame/parent', "Changed focus to the parent frame");
  }

  /**
   * Set cookie.
   *
   * @param string $name
   *   The name of the cookie to set.
   * @param string $value
   *   The value to set the cookie to.
   * @param string $path
   *   (Optional) The cookie's path.
   * @param string $domain
   *   (Optional) The domain the cookie is visible to.
   * @param bool $secure
   *   Whether or not the cookie is secure.
   * @param bool $http_only
   *   Whether the cookie is httpOnly.
   * @param int $expiry
   *   The expiration date of the cookie, in Unix time.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/cookie
   */
  public function setCookie($name, $value, $path = NULL, $domain = NULL, $secure = FALSE, $http_only = FALSE, $expiry = NULL) {
    $variables = array(
      'cookie' => array(
        'name' => $name,
        'value' => $value,
        'httpOnly' => $http_only,
      ),
    );
    if (!is_null($path)) {
      $variables['cookie']['path'] = $path;
    }
    if (!is_null($domain)) {
      $variables['cookie']['domain'] = $domain;
    }
    if (!is_null($secure)) {
      $variables['cookie']['secure'] = $secure;
    }
    if (!is_null($expiry)) {
      $variables['cookie']['expiry'] = $expiry;
    }
    return $this->setterHelper('/session/:sessionId/cookie', "Set cookie $name to value $value", $variables);
  }

  /**
   * Delete all cookies.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/cookie
   */
  public function deleteAllCookies() {
    return $this->setterHelper('/session/:sessionId/cookie', "Deleted all cookies", NULL, 'DELETE');
  }

  /**
   * Delete cookie.
   *
   * @param string $name
   *   The name of the cookie to delete.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/cookie/:name
   */
  public function deleteCookie($name) {
    return $this->setterHelper("/session/:sessionId/cookie/$name", "Deleted cookie $name", NULL, 'DELETE');
  }

  /**
   * Inject a snippet of JavaScript into the page.
   *
   * The script is executed in the context of the currently selected frame. It
   * is assumed to be synchronous, and the result of evaluating the script is
   * returned to the client.
   *
   * The $javascript argument defines the script to execute in the form of a
   * function body. The value returned by that function will be returned to the
   * client. The function will be invoked with the provided args array and the
   * values may be accessed via the arguments object in the order specified.
   *
   * @param string $javascript
   *   The javascript function to execute.
   * @param array $arguments
   *   The script arguments
   *
   * @return mixed
   *   The result of the script.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/execute
   */
  public function executeJsSync($javascript, $arguments = array()) {
    $variables = array(
      "script" => $javascript,
      "args" => $arguments,
    );
    return $this->getterHelper('/session/:sessionId/execute', "Executed JavaScript snippet", 'POST', $variables);
  }

  /**
   * Inject a snippet of JavaScript into the page.
   *
   * The script is executed in the contextof the currently selected frame. It is
   * assumed to be asynchronous, and must signal that is done by invoking the
   * provided callback, which is always provided as the final argument to the
   * function. The value to this callback will be returned to the client.
   *
   * Asynchronous script commands may not span page loads. If an unload event is
   * fired while waiting for a script result, an error should be returned to the
   * client.
   *
   * The script argument defines the script to execute in the form of a function
   * body. The function will be invoked with the provided args array and the
   * values may be accessed via the arguments object in the order specified.
   * The final argument will always be a callback function that must be invoked
   * to signal that the script has finished.
   *
   * @param string $javascript
   *   The javascript function to execute.
   * @param array $arguments
   *   The script arguments
   *
   * @return mixed
   *   The result of the script.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/execute_async
   */
  public function executeJsAsync($javascript, $arguments = array()) {
    $variables = array(
      "script" => $javascript,
      "args" => $arguments,
    );
    return $this->getterHelper('/session/:sessionId/execute_async', "Executed asynchronous JavaScript snippet", 'POST', $variables);
  }

  /**
   * Move the cursor relative to the position of the mouse or an element.
   *
   * @param int $right
   *   The number of pixels to move the cursor to the right. Use negative
   *   numbers to move the cursor left.
   * @param int $down
   *   The number of pixels to move the cursor down. Use negative numbers to
   *   move the cursor down.
   * @param string $element
   *   An optional element ID to move relative from.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/moveto
   */
  public function moveCursor($right, $down, $element = NULL) {
    $variables = array(
      "xoffset" => $right,
      "yoffset" => $down,
      "element" => $element,
    );
    $message = is_string($element) ? "Offset the cursor by $right, $down pixels from element $element" : "Offset the cursor by $right, $down pixels";
    return $this->setterHelper('/session/:sessionId/moveto', $message, $variables);
  }

  /**
   * Returns the correct name of the given mouse button for use in messages.
   *
   * @param int $button
   *   The number of the button.
   *
   * @throws Exception
   *   If the given button is invalid.
   *
   * @return string
   *   The name of the given button.
   */
  protected function getMouseButtonName($button) {
    switch ($button) {
      case 0:
        return 'left';

      case 1:
        return 'middle';

      case 2:
        return 'right';

      default:
        throw new Exception("Given mouse button $button is not a valid mouse button.");
    }
  }

  /**
   * Click the given mouse button.
   *
   * @param int $button
   *   The button to click (0 for left, 1 for middle, 2 for right).
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/click
   */
  public function mouseClickButton($button) {
    $name = $this->getMouseButtonName($button);
    $variables = array("button" => $button);
    return $this->setterHelper('/session/:sessionId/click', "Clicked the $name mouse button", $variables);
  }

  /**
   * Click and hold the given mouse button.
   *
   * @param int $button
   *   The button to click (0 for left, 1 for middle, 2 for right).
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/buttondown
   */
  public function mouseClickHold($button) {
    $name = $this->getMouseButtonName($button);
    $variables = array("button" => $button);
    return $this->setterHelper('/session/:sessionId/buttondown', "Held down the $name mouse button", $variables);
  }

  /**
   * Release the given mouse button.
   *
   * @param int $button
   *   The button to click (0 for left, 1 for middle, 2 for right).
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/buttonup
   */
  public function mouseClickRelease($button) {
    $name = $this->getMouseButtonName($button);
    $variables = array("button" => $button);
    return $this->setterHelper('/session/:sessionId/buttonup', "Released the $name mouse button", $variables);
  }

  /**
   * Double click the left mouse button.
   *
   * @returns bool
   *   TRUE on pass, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/doubleclick
   */
  public function mouseClickDouble() {
    return $this->setterHelper('/session/:sessionId/doubleclick', "Double-clicked the left mouse button");
  }

  /**
   * Sends keystrokes to a JavaScript prompt() dialog.
   *
   * Throws an error if there is no current JS dialog.
   *
   * @param string $text
   *   The text to send to the prompt() dialog.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   */
  public function sendJSPromptText($text) {
    $variables = array('text' => $text);
    return $this->setterHelper('/session/:sessionId/alert_text', "Sent the keystrokes '$text' to the JavaScript prompt dialog", $variables);
  }

  /**
   * Accepts the currently displayed alert dialog.
   *
   * Usually, this is equivalent to clicking on the 'OK' button in the dialog.
   *
   * Throws an error if there is no current JS dialog.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   */
  public function acceptJSAlert() {
    return $this->setterHelper('/session/:sessionId/accept_alert', "Accepted the JavaScript alert dialog");
  }

  /**
   * Dismisses the currently displayed alert dialog.
   *
   * For confirm() and prompt() dialogs, this is equivalent to clicking the
   * 'Cancel' button. For alert() dialogs, this is equivalent to clicking the
   * 'OK' button.
   *
   * Throws an error if there is no current JS dialog.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   */
  public function dismissJSAlert() {
    return $this->setterHelper('/session/sessionId/dismiss_alert', "Dismissed the JavaScript alert dialog");
  }

  /**
   * Element functions.
   */

  /**
   * Functions that get parameters/attributes from the element.
   */

  /**
   * Helper function for using getters on elements.
   *
   * @param SeleniumWebElement $element
   *   The element to execute from.
   * @param string $path
   *   The relative path to the suffix for the executor.
   * @param string $message
   *   The message to pass on failure. Use %element% within the string to insert
   *   the element's locator information (e.g. if the locator is 'name=title',
   *   %element% will be replaced with 'name: title').
   * @param string $http_method
   *   The HTTP method to use; defaults to GET.
   * @param null|array $variables
   *   An array of variables to use; use NULL for none.
   * @param null|string $attribute
   *   An attribute to pull from the JSON value array; use NULL for none.
   *
   * @return mixed|bool|null
   *   The result from the response, or FALSE on failure. NULL if the element
   *   passed in was invalid.
   */
  protected function elementGetterHelper(SeleniumWebElement $element, $path, $message, $http_method = 'GET', $variables = NULL, $attribute = NULL) {
    if (!($element instanceof SeleniumWebElement)) {
      // If the element passed in was invalid, the message saying so has
      // already been passed through, and we don't need to take orders.
      return NULL;
    }
    try {
      $response = $element->execute($http_method, $path, $variables);
      $result = $this->driver->getJSONValue($response, $attribute);
      $locator = $element->getLocatorParts();
      $message = str_replace('%element%', "{$locator['attribute']}: {$locator['value']}", $message);
    }
    catch (Exception $e) {
      $this->fail("$message: $e", 'Selenium WebDriver');
      return FALSE;
    }
    if ($this->verbose) {
      $this->pass("$message.", 'Selenium WebDriver');
    }
    return $result;
  }

  /**
   * Describes the identified element.
   *
   * @param SeleniumWebElement $element
   *   The element to describe.
   *
   * @return array
   *   The value array from the response.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id
   */
  public function describeElement(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '', "Described element (%element%)");
  }

  /**
   * Returns the visible text for the element.
   *
   * @param SeleniumWebElement $element
   *   The element to get the text for.
   *
   * @return array
   *   The value array from the response.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/text
   */
  public function getElementText(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/text', "Got text for element (%element%)");
  }

  /**
   * Query for the value of an element, as determined by its value attribute.
   *
   * @param SeleniumWebElement $element
   *   The element to get the value of.
   *
   * @return string|NULL
   *   The element's value, or null if it does not have a value attribute.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/value
   */
  public function getElementValue(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/attribute/value', "Got the value of element (%element%)");
  }

  /**
   * Determine if an element is currently displayed.
   *
   * @param SeleniumWebElement $element
   *   The element to check if it is displayed.
   *
   * @return bool
   *   Whether the element is displayed.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/displayed
   */
  public function isElementDisplayed(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/displayed', "Checked if element (%element%) was displayed");
  }

  /**
   * Determine if an element is currently enabled.
   *
   * @param SeleniumWebElement $element
   *   The element to check if it is enabled.
   *
   * @return bool
   *   Whether the element is enabled.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/enabled
   */
  public function isElementEnabled(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/enabled', "Checked if element (%element%) was enabled");
  }

  /**
   * Determine status of an OPTION element, or a checkbox/radio INPUT element.
   *
   * @param SeleniumWebElement $element
   *   The element to check the status of.
   *
   * @return bool
   *   Whether the element is selected.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/selected
   */
  public function isElementSelected(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/selected', "Checked if element (%element%) was selected");
  }

  /**
   * Search for an element on the page, starting from the identified element.
   *
   * The located element will be returned as a SeleniumWebElement JSON object.
   * Each locator must return the first matching element located in the DOM.
   *
   * @param SeleniumWebElement $element
   *   The element to search from.
   * @param string $locator
   *   The locator to use when getting the next element.
   *
   * @return SeleniumWebElement
   *   A SeleniumWebElement JSON object for the located element.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/element
   */
  public function getNextElement(SeleniumWebElement $element, $locator) {
    $variables = $this->driver->ParseLocator($locator);
    $next_element_id = $this->elementGetterHelper($element, '/element', "Got next element from element (%element%)", 'POST', $variables, 'ELEMENT');
    return new SeleniumWebElement($this->driver, $next_element_id, $locator);
  }

  /**
   * Search for elements on the page, starting from the identified element.
   *
   * The located elements will be returned as a SeleniumWebElement JSON objects.
   * Elements should be returned in the order located in the DOM.
   *
   * @param SeleniumWebElement $element
   *   The element to search from.
   * @param string $locator
   *   The locator to use when getting all next elements.
   *
   * @return SeleniumWebElement[]
   *   An array of SeleniumWebElement JSON objects for the located elements.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/elements
   */
  public function getAllNextElements(SeleniumWebElement $element, $locator) {
    $variables = $this->driver->ParseLocator($locator);
    $next_element_ids = $this->elementGetterHelper($element, '/elements', "Got all next elements from element (%element%)", 'POST', $variables, 'ELEMENT');
    $all_elements = array();
    foreach ($next_element_ids as $element_id) {
      $all_elements[] = new SeleniumWebElement($this->driver, $element_id, $locator);
    }
    return $all_elements;
  }

  /**
   * Query for an element's tag name.
   *
   * @param SeleniumWebElement $element
   *   The element to query
   *
   * @return string
   *   The element's tag name, as a lowercase string.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/name
   */
  public function getElementTagName(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/name', "Queried the tag name of element (%element%)");
  }

  /**
   * Get the value of an element's attribute.
   *
   * @param SeleniumWebElement $element
   *   The element to get the value from.
   * @param string $attribute_name
   *   The name of the attribute to get the value of.
   *
   * @return string|NULL
   *   The value of the attribute, or null if it is not set on the element.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/attribute/:name
   */
  public function getElementAttributeValue(SeleniumWebElement $element, $attribute_name) {
    return $this->elementGetterHelper($element, "/attribute/$attribute_name", "Got the value of attribute $attribute_name for element (%element%)");
  }

  /**
   * Test if two element IDs refer to the same DOM element.
   *
   * @param SeleniumWebElement $element
   *   The element to compare from.
   * @param string $other_element_id
   *   The ID of the element to check against.
   *
   * @return bool
   *   Whether the two IDs refer to the same element.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/equals/:other
   */
  public function elementIsSameElementAs(SeleniumWebElement $element, $other_element_id) {
    return $this->elementGetterHelper($element, "/equals/$other_element_id", "Compared element (%element%) to element with ID $other_element_id");
  }

  /**
   * Determine an element's location on the page.
   *
   * The point (0, 0) refers to the upper-left corner of the page. The element's
   * coordinates are returned as an array with x and y properties.
   *
   * @param SeleniumWebElement $element
   *   The element to locate on the page.
   *
   * @return array
   *   The coordinates of the element on the page, as array(int x, int y).
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/location
   */
  public function getElementLocation(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/location', "Got the location of element (%element%)");
  }

  /**
   * Determine an element's size, in pixels.
   *
   * @param SeleniumWebElement $element
   *   The element to get the size of.
   *
   * @return array
   *   The size of the element, in pixels, as array(int width, int height).
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/size
   */
  public function getElementSize(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/size', "Got the size of element (%element%)");
  }

  /**
   * Query the value of an element's computed CSS property.
   *
   * The CSS property to query should be specified using the CSS property name,
   * not the JavaScript property name (e.g., background-color instead of
   * backgroundColor).
   *
   * @param SeleniumWebElement $element
   *   The element to get the CSS property from.
   * @param string $property_name
   *   The property name to query for.
   *
   * @return string
   *   The value of the specified CSS property.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/css/:propertyName
   */
  public function getElementCSSProperty(SeleniumWebElement $element, $property_name) {
    return $this->elementGetterHelper($element, "/css/$property_name", "Got the value of the CSS property $property_name for element (%element%)");
  }

  /**
   * Functions to set various attributes/parameters of an element.
   */

  /**
   * Helper for performing element setter functions.
   *
   * @param SeleniumWebElement $element
   *   The element to set an attribute or parameter for.
   * @param string $path
   *   The relative URL to paste on the end of the request.
   * @param string $message
   *   The message to pass through at the end.
   * @param array|null $variables
   *   Any variables required by the request executor. Use NULL for none.
   * @param string $http_method
   *   The HTTP method to use; defaults to POST.
   *
   * @return bool|null
   *   TRUE on success, or FALSE on failure. NULL if the element passed in was
   *   invalid.
   */
  protected function elementSetterHelper(SeleniumWebElement $element, $path, $message, $variables = NULL, $http_method = 'POST') {
    if (!($element instanceof SeleniumWebElement)) {
      // In this case, we just skip doing anything with the element.
      return NULL;
    }
    try {
      $element->execute($http_method, $path, $variables);
      $locator = $element->getLocatorParts();
      $message = str_replace('%element%', "{$locator['attribute']}: {$locator['value']}", $message);
    }
    catch (Exception $e) {
      $this->fail("$message: $e", "Selenium WebDriver");
      return FALSE;
    }
    $this->pass("$message.", "Selenium WebDriver");
    return TRUE;
  }

  /**
   * Submit a FORM element.
   *
   * @param SeleniumWebElement $element
   *   The element to submit from.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   *
   * The submit command may also be applied to any element that is a descendant
   * of a FORM element.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/submit
   */
  public function submitElement(SeleniumWebElement $element) {
    return $this->elementSetterHelper($element, '/submit', "Submitted form from element (%element%)");
  }

  /**
   * Click on an element.
   *
   * @param SeleniumWebElement $element
   *   The element to click on.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/click
   */
  public function clickElement(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/click', "Clicked on element (%element%)");
  }

  /**
   * Clear a TEXTAREA or text INPUT element's value.
   *
   * @param SeleniumWebElement $element
   *   The element to clear.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/clear
   */
  public function clearElement(SeleniumWebElement $element) {
    return $this->elementSetterHelper($element, '/clear', "Cleared element (%element%)");
  }

  /**
   * Select an OPTION element, or a checkbox/radio INPUT element.
   *
   * @param SeleniumWebElement $element
   *   The element to select.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/selected
   */
  public function selectElement(SeleniumWebElement $element) {
    return $this->elementSetterHelper($element, '/selected', "Selected element (%element%)");
  }

  /**
   * Toggle whether an OPTION or checkbox/radio INPUT element is selected.
   *
   * Uses the elementGetterHelper, as the result from this is useful for knowing
   * whether or not the element is now selected.
   *
   * @param SeleniumWebElement $element
   *   The element to toggle.
   *
   * @return bool
   *   Whether the element is selected after toggling its state.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/toggle
   */
  public function toggleElement(SeleniumWebElement $element) {
    return $this->elementGetterHelper($element, '/toggle', "Toggled element (%element%)", 'POST');
  }

  /**
   * Query for the value of an element, as determined by its value attribute.
   *
   * @param SeleniumWebElement $element
   *   The element to send keys to.
   * @param string $keys
   *   The keys to send to the element.
   *
   * @return bool
   *   TRUE on success, FALSE on failure.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/value
   */
  public function sendKeysToElement(SeleniumWebElement $element, $keys) {
    $variables = array(
      "value" => preg_split('//u', $keys, -1, PREG_SPLIT_NO_EMPTY),
    );
    return $this->elementSetterHelper($element, '/value', "Sent the keys '$keys' to element (%element%)", $variables);
  }

  /**
   * Drag and drop an element.
   *
   * The distance to drag the element should be specified relative to the upper-
   * left corner of the page. For this reason, it is best to obtain the position
   * of the element before dragging it, and setting $pixels_right/$pixels_down
   * relative to the obtained position.
   *
   * @param SeleniumWebElement $element
   *   The element to drag and drop.
   * @param int $pixels_right
   *   The number of pixels to drag the element horizontally. Use negative
   *   numbers to drag the element to the left.
   * @param int $pixels_down
   *   The number of pixels to drag the element vertically. Use negative numbers
   *   to drag the element up.
   *
   * @see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/drag
   */
  public function dragAndDropElement(SeleniumWebElement $element, $pixels_right, $pixels_down) {
    $this->moveCursor(0, 0, $element);
    $this->mouseClickHold(0);
    $this->moveCursor($pixels_right, $pixels_down);
    $this->mouseClickRelease(0);
  }

  /*
   * Functions for getting attributes/parameters of <select> elements.
   */

  /**
   * Search for the selected option of a <select> element on the page.
   *
   * The located element will be returned as a SeleniumWebElement JSON object.
   *
   * @param SeleniumWebElement $element
   *   The element to find the selected option for.
   *
   * @return SeleniumWebElement
   *   A SeleniumWebElement JSON object for the located element.
   */
  public function getElementSelectedOption(SeleniumWebElement $element) {
    try {
      // Does not work in IE8.
      return $this->getNextElement($element, "css=option[selected]");
    }
    catch (Exception $e) {
      // Doesn't work in IE7.
      return $this->getNextElement($element, "css=option[selected='selected']");
    }
  }

  /**
   * Search for options for <select> element on the page.
   *
   * Starts from the identified element. The located elements will be returned
   * as a SeleniumWebElement JSON object. Elements should be returned in the
   * order located in the DOM.
   *
   * @param SeleniumWebElement $element
   *   The element to get options for.
   *
   * @return SeleniumWebElement[]
   *   A list of SeleniumWebElement JSON objects for the located elements.
   */
  public function getOptions(SeleniumWebElement $element) {
    return $this->getAllNextElements($element, "tag name=option");
  }

  /**
   * Functions to set parameters/attributes for <select> elements.
   */

  /**
   * Search for <select> element on the page specified by the option label.
   *
   * Starts from the identified element.
   *
   * @param SeleniumWebElement $element
   *   The element to select an option in.
   * @param string $label
   *   The label of the option for select element.
   *
   * @return bool
   *   TRUE on success, or FALSE on failure.
   */
  public function selectElementOptionByLabel(SeleniumWebElement $element, $label) {
    $option_element = $this->getNextElement($element, "xpath=//option[text()='$label']");
    return $this->selectElement($option_element);
  }

  /**
   * Search for <select> element on the page specified by the option value.
   *
   * Starts from the identified element.
   *
   * @param SeleniumWebElement $element
   *   The element to select an option in.
   * @param string $value
   *   The value of the option for select element.
   *
   * @return bool
   *   TRUE on success, or FALSE on failure.
   */
  public function selectElementOptionByValue(SeleniumWebElement $element, $value) {
    $option_element = $this->getNextElement($element, "xpath=//option[@value='$value']");
    return $this->selectElement($option_element);
  }

  /**
   * Search for <select> element on the page specified by the option index.
   *
   * Starts from the identified element.
   *
   * @param SeleniumWebElement $element
   *   The element to select an option in.
   * @param string $index
   *   The index of the option for select element.
   *
   * @return bool
   *   TRUE on success, or FALSE on failure.
   */
  public function selectElementOptionByIndex(SeleniumWebElement $element, $index) {
    $option_element = $this->getNextElement($element, "xpath=//option[$index]");
    return $this->selectElement($option_element);
  }
}
