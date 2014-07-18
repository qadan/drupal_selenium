<?php
/**
 * @file
 * Web element class for Selenium web driver.
 */

/**
 * Class representing a Selenium web element.
 */
class SeleniumWebElement {

  /**
   * The SeleniumWebDriver associated with this element.
   *
   * @var SeleniumWebDriver
   */
  protected $driver;

  /**
   * ID of the element to route the command to.
   *
   * @var string
   */
  public $id;

  /**
   * Locator returned from the first matching element located in the DOM.
   *
   * @var string
   */
  protected $locator;

  /**
   * UTF-8 codes for keys understood by Selenium.
   *
   * @var array
   */
  protected static $keys = array(
    'NullKey' => "\uE000",
    'CancelKey' => "\uE001",
    'HelpKey' => "\uE002",
    'BackspaceKey' => "\uE003",
    'TabKey' => "\uE004",
    'ClearKey' => "\uE005",
    'ReturnKey' => "\uE006",
    'EnterKey' => "\uE007",
    'ShiftKey' => "\uE008",
    'ControlKey' => "\uE009",
    'AltKey' => "\uE00A",
    'PauseKey' => "\uE00B",
    'EscapeKey' => "\uE00C",
    'SpaceKey' => "\uE00D",
    'PageUpKey' => "\uE00E",
    'PageDownKey' => "\uE00F",
    'EndKey' => "\uE010",
    'HomeKey' => "\uE011",
    'LeftArrowKey' => "\uE012",
    'UpArrowKey' => "\uE013",
    'RightArrowKey' => "\uE014",
    'DownArrowKey' => "\uE015",
    'InsertKey' => "\uE016",
    'DeleteKey' => "\uE017",
    'SemicolonKey' => "\uE018",
    'EqualsKey' => "\uE019",
    'Numpad0Key' => "\uE01A",
    'Numpad1Key' => "\uE01B",
    'Numpad2Key' => "\uE01C",
    'Numpad3Key' => "\uE01D",
    'Numpad4Key' => "\uE01E",
    'Numpad5Key' => "\uE01F",
    'Numpad6Key' => "\uE020",
    'Numpad7Key' => "\uE021",
    'Numpad8Key' => "\uE022",
    'Numpad9Key' => "\uE023",
    'MultiplyKey' => "\uE024",
    'AddKey' => "\uE025",
    'SeparatorKey' => "\uE026",
    'SubtractKey' => "\uE027",
    'DecimalKey' => "\uE028",
    'DivideKey' => "\uE029",
    'F1Key' => "\uE031",
    'F2Key' => "\uE032",
    'F3Key' => "\uE033",
    'F4Key' => "\uE034",
    'F5Key' => "\uE035",
    'F6Key' => "\uE036",
    'F7Key' => "\uE037",
    'F8Key' => "\uE038",
    'F9Key' => "\uE039",
    'F10Key' => "\uE03A",
    'F11Key' => "\uE03B",
    'F12Key' => "\uE03C",
    'CommandKey' => "\uE03D",
    'MetaKey' => "\uE03D",
  );

  /**
   * Constructs a SeleniumWebElement.
   *
   * @param SeleniumWebDriver $driver
   *   The driver this element belongs to.
   * @param string $element_id
   *   The ID of the element.
   * @param string $locator
   *   The locator for this element.
   */
  public function __construct(SeleniumWebDriver $driver, $element_id, $locator) {
    $this->driver = $driver;
    $this->id = $element_id;
    $this->locator = $locator;
  }

  /**
   * Returns the $locator property.
   */
  public function getLocatorParts() {
    $parts = explode("=", $this->locator);
    return array(
      'attribute' => $parts[0],
      'value' => $parts[1],
    );
  }

  /**
   * Executes a driver command from within the context of this element.
   *
   * @param string $http_type
   *   The type of HTTP request to make (GET, POST, DELETE, etc.).
   * @param string $relative_url
   *   The portion of the HTTP request to make that comes adter the element ID.
   * @param array|null $variables
   *   Any variables to be passed to the function to execute.
   *
   * @return array
   *   The response from the server.
   */
  public function execute($http_type, $relative_url, $variables = NULL) {
    return $this->driver->execute($http_type, "/session/:sessionId/element/" . $this->id . $relative_url, $variables);
  }

  /**
   * Helper function to get a key from self::$keys.
   *
   * @param string $key_name
   *   The name of the key to get.
   *
   * @throws Exception
   *   If unable to type the given key name.
   *
   * @return string
   *   The given key.
   */
  public function getKey($key_name) {
    if (isset(self::$keys[$key_name])) {
      return json_decode('"' . self::$keys[$key_name] . '"');
    }
    else {
      throw new Exception("Can't type key $key_name");
    }
  }
}
