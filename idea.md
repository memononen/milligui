MilliGUI is simple stupid UI library for editors and prootypes.
- the data binding uses IMGUI
- widgets are composed of elements like in HTML
- layout works like flex-box
- styling is done using css like selectors

Layout and data binding are done using the usual IMGUI style coding,
styling is done using predefined styles, matched using simple selectors.


- basic elements
	- panel (absolute div)
	- box (flex box div)
	- popup (relative div)
	- text
	- image
	- input (text)
	- slider 

- components
  - button
  - select
  - toggle
  - button group
  - tab
  - numner input
  - color input
  - vec3 input

- layout parameter for box
	- width <#, auto>
		- width of the box
		- default is auto (fit to content)
	- height: <#, auto>
		- height of the box
		- default is auto (fit to content)
	- dir: <row, col>
		- direction in which the child elements are stacked in
		- defines main direction (cross direction is the opposite)
	- grow: <#>
		- controls how much the element is grown to fit into the free parent space
		- default 0 (don’t grow)
	- align: <start, end, centre, justify>
		- how to align in cross dir
	- overflow: <fit, hidden, scroll>
		- how to handle overflow in main direction (cross is always fit/shrink content)		
	- logic <none, click, drag, type>
		- what kind of logic to apply to the element
	- class <style>
		- visual representation of the element

- presentation parameters
	- panel & box
		- padding x, y
		- spacing (margin right/bottom in main dir)
		- fill colour
		- border color
		- border radius
		- corner radius
	- text
		- font face
		- font size
		- font weight
		- text color
	- image
		- border color
		- border radius
		- corner radius
	- input
		- font face
		- font size
		- font weight
		- text color
	- slider
		- handle size
		- handle fill colour
		- handle border color
		- handle border radius
		- handle corner radius
		- slot color
		- slot size

- presentation style selector
	- each element has class
	- selector tries to match the longest definition starting from right
	- use wildcard * to match anything in between
	- example:
		- “text” matches any text
		- “button text” matches text inside buttons
		- “properties * button” matches button inside properties box/panel
		- “properties * button text” matches button text inside properties box/panel
	- selectors work using hashes
	- styles don’t cascade


