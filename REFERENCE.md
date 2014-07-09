# SELENIUM WEB DRIVER REFERENCE GUIDE

Because of the sheer number of endpoints available to you when using the Selenium Web Driver, we provide here a quick, handy reference guide to sort through all the methods in DrupalSeleniumWebTestCase, categorized by functionality.

These methods are available when writing tests by calling `$this->functionName()`.

## Assertions

Name|Description
----|-----------
assertFieldById|Asserts the existence of a field by ID.
assertFieldByName|Asserts the existence of a field by name.
assertFieldByXPath|Asserts the existence of a field by the given XPath.
assertFieldChecked|Asserts that a checkbox is checked.
assertField|Asserts that a field exists on the page.
assertLinkByHref|Asserts that a link exists on the page, by full or partial href.
assertLink|Asserts that a link exists on the page.
assertNoDuplicateIds|Asserts that there are no duplicate IDs on the page.
assertNoFieldById|Asserts the absence of a field by ID.
assertNoFieldByName|Asserts the absence of a field by name.
assertNoFieldChecked|Asserts that a checkbox is unchecked.
assertNoField|Asserts that a field doesn't exist on the page.
assertNoLinkByHref|Asserts that no link exists on the page, by full or partial href.
assertNoLink|Asserts that a link doesn't exist on the page.
assertNoOptionSelected|Asserts that an option is not selected.
assertNoTitle|Asserts that the given value is not the page title.
assertOptionSelected|Asserts that an option is selected.
assertTextHelper|Asserts that text does or doesn't exist on the page.
assertTitle|Asserts the title of the page.
assertUrl|Asserts the URL of the page.

## Get Global/Browser Properties

Name|Description
----|-----------
getAllWindowHandles|Gets all browser window handles.
getScreenshot|Gets an image of the browser in its current state.
getWindowHandle|Gets the handle of the current borwser window.

## Set Window Properties

Name|Description
----|-----------
setAsyncTimeout|Set the asynchronous JS script timeout.
setCookie|Set a cookie for the current window.
setImplicitWait|Set the implicit wait time for scripts.

## Use Browser Functionality

Name|Description
----|-----------
closeWindow|Close the current window.
deleteAllCookies|Delete all cookies for the current window.
deleteCookie|Delete a specified cookie.
executeJsAsync|Execute an asynchronous JavaScript script.
executeJsSync|Execute a synchronous JavaScript script.
historyBack|Move backward in the browser history.
historyForward|Move forward in the browser history.
refresh|Refresh the current window.
selectFrame|Select a given frame in the current window.
selectParentFrame|Select the parent of the current frame.
selectWindow|Select a given window.

## Get Page Properties

Name|Description
----|-----------
getActiveElement|Gets all active page elements.
getAllCookies|Gets all the page cookies.
getAllElements|Gets all elements on the page.
getBodyText|Gets the text of the body.
getCookie|Gets a single cookie by given properties.
getElement|Gets an element on the page.
getPageTitle|Gets the title of the page.
getSource|Gets the page source.
getUrl|Gets the current URL.
waitForElements|Waits to see if an element shows up on the page.
waitForVisibleElements|Waits to see if an element becomes visible on the page.

## Mouse Functionality

Name|Description
----|-----------
mouseClickButton|Click a mouse button.
mouseClickDouble|Double-click the left mouse button.
mouseClickHold|Hold down a mouse button.
mouseClickRelease|Release a mouse button.
moveCursor|Move the cursor relative to an element or to its current position.

## Get Element Properties

Name|Description
----|-----------
describeElement|Get properties of an element as an array.
elementIsSameElementAs|Determine if two element locators represent the same element.
getAllNextElements|Get all elemenets after the given one using specific search parameters.
getElementAttributeValue|Get the value of a specified attribute of an element.
getElementCSSProperty|Get a specified CSS property of an element.
getElementLocation|Get the coordinates of an element.
getElementSelectedOption|Get the currently selected option of an element.
getElementSize|Get the sixe of an element.
getElementTagName|Get the tag name of an element.
getElementText|Get the text of an element.
getElementValue|Get the value of an element.
getNextElement|Get the next element after the given one using specific search parameters.
getOptions|Get all an element's options.
isElementDisplayed|Determine if the element is displayed.
isElementEnabled|Determine if the element is enabled.
isElementSelected|Determine if the element is selected.

## Use/Set Elements

Name|Description
----|-----------
clearElement|Clear an element.
clickElement|Click an element.
dragAndDropElement|Drag and drop an element by an offset.
selectElementOptionByIndex|Select a 'select'-type element option by its index.
selectElementOptionByLabel|Select a 'select'-type element option by its label.
selectElementOptionByValue|Select a 'select'-type element option by its value.
selectElement|Select an element.
sendKeysToElement|Send a string of keys to an element.
submitElement|Submit a form from an element.
toggleElement|Toggle an element.

## JavaScript Dialog Box Interaction

Name|Description
----|-----------
acceptJSAlert|Accept a JS dialog box.
dismissJSAlert|Dismiss a JS dialog box.
getJSDialogText|Gets the text displayed in a JS dialog box.
sendJSPromptText|Fill in a JS prompt() dialog box and submit it.