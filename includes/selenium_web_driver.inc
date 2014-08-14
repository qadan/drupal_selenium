<?php
/**
 * @file
 * Selenium Web Driver class and wrapper.
 */

/**
 * Class of the connection to Webdriver.
 */
class SeleniumWebDriver {

  /**
   * The ID of this session.
   *
   * @var string
   */
  protected $sessionId;

  /**
   * Array-formatted JSON obtained from the initialization of this session.
   *
   * @var array
   */
  public $sessionResponse;

  /**
   * Destructor; closes the session when the driver is no longer needed.
   */
  public function __destruct() {
    $this->execute("DELETE", "/session/:sessionId");
  }

  /**
   * Executes a call to server.
   *
   * @param string $http_type
   *   The cURL option to be passed to CURLOPT_CUSTOMREQUEST (POST, GET, etc.)
   * @param string $relative_url
   *   The portion of the URL after /wd/hub.
   * @param string $variables
   *   Variables to be passed to CURLOPT_POSTFIELDS for POST or PUT requests.
   *
   * @return array
   *   The response from the server.
   */
  public function execute($http_type, $relative_url, $variables = NULL) {
    // JSON encode any POST/PUT variables.
    if ($variables !== NULL) {
      $variables = json_encode($variables);
    }

    // Determine the URL to make the cURL request to.
    $relative_url = str_replace(':sessionId', $this->sessionId, $relative_url);
    $full_url = SELENIUM_SERVER_URL . $relative_url;

    // Perform the cURL request.
    $curl = curl_init($full_url);
    curl_setopt($curl, CURLOPT_CUSTOMREQUEST, $http_type);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, TRUE);
    curl_setopt($curl, CURLOPT_HEADER, TRUE);
    // Add in any variables.
    if (($http_type === "POST" || $http_type === "PUT") && $variables !== NULL) {
      curl_setopt($curl, CURLOPT_POSTFIELDS, $variables);
    }
    $full_response = curl_exec($curl);
    curl_close($curl);

    // Now that we have the results, split it into a header and body.
    $response_parts = explode("\r\n\r\n", $full_response, 2);
    $response['header'] = $response_parts[0];
    if (!empty($response_parts[1])) {
      $response['body'] = $response_parts[1];
    }

    // Check the response to ensure it is valid and zero.
    if (isset($response['body'])) {
      $this->checkResponseStatus($response['body'], $variables);
    }

    return $response;
  }

  /**
   * Checks the status of the server response.
   *
   * Throws an exception if there was trouble.
   *
   * @param string $body
   *   String of JSON representing the body of the server response.
   * @param string $variables
   *   A string of variables that may need to to be passed to the exception
   *   message.
   *
   * @throws Exception
   *   If the response code os non-zero, throw the error.
   */
  protected function checkResponseStatus($body, $variables) {
    $array = json_decode(trim($body), TRUE);
    if (!is_null($array)) {
      $status_codes = $this->getStatusCodes();
      $response_status_code = $array["status"];
      if (!isset($status_codes[$response_status_code])) {
        throw new Exception("Unknown status code $response_status_code returned from server.\n$body");
      }
      if (!in_array($response_status_code, array(0, 303))) {
        $message = $response_status_code . " - " . $status_codes[$response_status_code][0] . " - " . $status_codes[$response_status_code][1] . "\n";
        $message .= "Arguments: " . print_r($variables, TRUE) . "\n";
        if (isset($array['value']['message'])) {
          $message .= "Message: " . $array['value']['message'] . "\n";
        }
        else {
          $message .= "Response: " . $body . "\n";
        }
        throw new Exception($message);
      }
    }
  }

  /**
   * Parses a locator to be used when finding an element.
   *
   * Also ensures compatibility between selectors generated using different
   * versions of Selenium.
   *
   * @param string $locator
   *   The locator to parse.
   *
   * @return array
   *   The locator as a properly-formatted variable array for getting elements.
   *
   * @throws Exception
   *   In cases where an invalid locator is used.
   */
  public function parseLocator($locator) {
    // Used to convert old Selenium locators to Selenium 2 compatible ones.
    $se1_to_se2 = array(
      "identifier" => "id",
      "id" => "id",
      "name" => "name",
      "xpath" => "xpath",
      "link" => "link text",
      "css" => "css selector",
      // Selenium 2 has these four new selectors. Also, Selenium 1 is missing
      // the 'dom' selector.
      0 => "partial link text",
      1 => "tag name",
      2 => "class",
      3 => "class name",
    );

    $locator_parts = explode("=", $locator, 2);
    // Explicit Selelenium 1 selectors.
    if (array_key_exists($locator_parts[0], $se1_to_se2) && $locator_parts[1]) {
      $strategy = $se1_to_se2[$locator_parts[0]];
      $value = $locator_parts[1];
    }
    // Explicit Selenium 2 selectors.
    elseif (in_array($locator_parts[0], $se1_to_se2) && $locator_parts[1]) {
      $strategy = $locator_parts[0];
      $value = $locator_parts[1];
    }
    // Attempts to guess the selector based on Selenium 1 standards.
    elseif (substr($locator, 0, 2) === "//") {
      $strategy = "xpath";
      $value = $locator;
    }
    // Handle an attempted DOM selector.
    elseif (substr($locator, 0, 9) === "document." || substr($locator, 0, 4) === "dom=") {
      throw new Exception("DOM selectors aren't supported in WebDriver: $locator");
    }
    // In all other cases, fall back to the ID.
    else {
      $strategy = "id";
      $value = $locator;
    }
    return array("using" => $strategy, "value" => $value);
  }

  /**
   * Gets the "value" array from a Selenium JSON response body.
   *
   * This can contain all sorts of goodies we would like to use in various
   * situations.
   *
   * @param array $curl_response
   *   The response returned from cURL, with header/body keys.
   * @param string $attribute
   *   An optional single attribute to extract from the array.
   *
   * @return array|string
   *   An array of attributes from the value array, or a string representing
   *   a single attribute specified for extraction, or an array of multiple
   *   second-level results found.
   *
   * @throws Exception
   *   If anything goes wrong in the extraction process.
   */
  public function getJSONValue($curl_response, $attribute = NULL) {
    // If there's no body, we have a problem right from the get-go.
    if (!isset($curl_response['body'])) {
      throw new Exception("Response had no body.\n{$curl_response['header']}");
    }

    // Convert the body from JSON to an array.
    $array = json_decode(trim($curl_response['body']), TRUE);
    // Throw an exception if something went wrong trying to decode the body.
    if ($array === NULL) {
      throw new Exception("Body could not be decoded as JSON\n{$curl_response['body']}");
    }
    // Throw an exception if the value array is missing.
    if (!isset($array["value"])) {
      if (is_null($array['value'])) {
        $array['value'] = array();
      }
      else {
        throw new Exception("JSON had no value array.");
      }
    }

    // If $attribute is null, just return the whole value array.
    if ($attribute === NULL) {
      $rv = $array["value"];
    }
    // Otherwise, it's time to try to extract the attribute.
    else {
      // If it's found on the top level, we're good.
      if (isset($array["value"][$attribute])) {
        $rv = $array["value"][$attribute];
      }
      // If it's not found on the top level, we go one level deeper to find it.
      elseif (is_array($array["value"])) {
        $rv = array();
        foreach ($array["value"] as $a_value) {
          if (isset($a_value[$attribute])) {
            $rv[] = $a_value[$attribute];
          }
          // Make a stink if nothing is found.
          else {
            throw new Exception("JSON value did not have attribute $attribute\n" . $array["value"]["message"]);
          }
        }
      }
      else {
        throw new Exception("JSON value did not have attribute $attribute\n" . $array["value"]["message"]);
      }
    }
    return $rv;
  }

  /**
   * Returns status codes via a function.
   *
   * For no other reason than because Coder is being a butt about accessing
   * camelCase-y properties statically.
   */
  public static function getStatusCodes() {
    return array(
      0 => array("Success", " The command executed successfully."),
      7 => array("NoSuchElement", " An element could not be located on the page using the given search parameters."),
      8 => array("NoSuchFrame", " A request to switch to a frame could not be satisfied because the frame could not be found."),
      9 => array("UnknownCommand", " The requested resource could not be found, or a request was received using an HTTP method that is not supported by the mapped resource."),
      10 => array("StaleElementReference", " An element command failed because the referenced element is no longer attached to the DOM."),
      11 => array("ElementNotVisible", " An element command could not be completed because the element is not visible on the page."),
      12 => array("InvalidElementState", " An element command could not be completed because the element is in an invalid state (e.g., attempting to click a disabled element)."),
      13 => array("UnknownError", " An unknown server-side error occurred while processing the command."),
      15 => array("ElementIsNotSelectable", " An attempt was made to select an element that cannot be selected."),
      17 => array("JavaScriptError", " An error occurred while executing user supplied JavaScript."),
      19 => array("XPathLookupError", " An error occurred while searching for an element by XPath."),
      23 => array("NoSuchWindow", " A request to switch to a different window could not be satisfied because the window could not be found."),
      24 => array("InvalidCookieDomain", " An illegal attempt was made to set a cookie under a different domain than the current page."),
      25 => array("UnableToSetCookie", " A request to set a cookie's value could not be satisfied."),
      28 => array("Timeout", " A command did not complete before its timeout expired."),
      303 => array("See other", "See other"),
    );
  }
}
