<?php // $Id: qbwc.inc,v 1.9 2009/08/03 00:06:13 vauxia Exp $

/**
 * @file
 * Connect to the Quickbooks Web Service
 *
 */

/**
 * QBWC page callback.
 */
function qbwc() {
  global $base_url;

  if (!isset($_SERVER['HTTP_SOAPACTION'])) {
    // No callback function has been defined, so return nothing.
    echo '';
  }
  else {
    // Pass the request to the SoapServer.
    $wsdl = dirname(__FILE__) .'/qbwc.wsdl';
    $qbwc = new SoapServer($wsdl, array('uri' => variable_get('qbwc_url', '')));
    $qbwc->setClass('qbwc');
    $qbwc->handle();
    return;
  }
}

class qbwc {

  // Identifier for this session.  May be used to log activities, etc.
  var $qbwc_id;

  // Session identifier for Drupal.
  var $sid;

  // We cannot get back the original Server SOAP object, but we can at least 
  // load the right session getting our session variables back.
  function _session($ticket) {
    global $user;

    // The user has already been authenticated for some reason.
    if ($user->uid) return;

    if ($row = db_fetch_object(db_query("SELECT * FROM {qbwc} 
      WHERE ticket = '%s' AND timestamp_end >= %d", $ticket, time()))) {

      $this->qbwc_id = $row->qbwc_id;
      $this->sid = $row->sid;

      // Remove any session that may have been started prior to this call.
      session_destroy();

      // Pretend that cookies are enabled so we can get back our session.
      $_COOKIE[session_name()] = $row->sid;
      session_id($row->sid);

      // Re-run the session initiation phase of drupal_bootstrap.
      _drupal_bootstrap(DRUPAL_BOOTSTRAP_SESSION);
      return TRUE;
    }

    // Invalid session
    return FALSE;
  }

  function authenticate($params) {
    $name = $params->strUserName;
    $pass = $params->strPassword;
    $hostname = ip_address();

    // Use the current session id as the ticket.
    $ticket = session_id();

    // Default result: "non-valid user credentials".
    $ret = 'nvu';

    // Unimplemented 3rd and 4th return values.
    $next_update = NULL;
    $minimum_wait = NULL;

    // Authenticate as the quickbooks drupal user
    if ($name && ($name == variable_get('qbwc_user', ''))) {
      if ($user = user_authenticate(array('name' => $name, 'pass' => $pass))) {

        // Store a reference to our session id and the ticket, so we can
        // get them back later.  Note that session_id != ticket by now.
        $qbwc = (object) array(
          'ticket' => $ticket,
          'sid' => session_id(),
          'uid' => $user->uid,
          'hostname' => ip_address(),
          'timestamp_start' => time(),
          'timestamp_end' => time() + 1800, // Limit the session to 30 minutes.
        );
        drupal_write_record('qbwc', $qbwc);

        $this->qbwc_id = $qbwc->qbwc_id;
        $this->sid = $qbwc->sid;

        // Initialize our session variable namespace and record the start time.
        // The timestamp will now be available via qbwc_get('time').
        $_SESSION['qbwc'] = array('time' => time());

        // Set nothing to do until a module decides it has something to do.
        $ret = 'none';

        // Let modules return whether they have work to do or not.
        if ($requests = module_invoke_all('qbwc_request')) {

          // Store our list of requests for later processing.
          foreach ($requests as $request) {
            qbwc_request_queue($request);
          }

          // Return company file or empty, which means "currently open file"
          $ret = variable_get('qb_company_file', '');
        }
      }
    }

    $result = array($ticket, $ret, $next_update, $minimum_wait);
    return (object) array('authenticateResult' => $result);
  }

  function closeConnection($params) {
    global $user;

    $this->_session($params->ticket);

    // Let modules know we are closing the session
    module_invoke_all('qbwc_close');

    // Close the session and log it just like user_logout() sans drupal_goto.
    if (variable_get('qbwc_debug', 0)) watchdog('user', t('Session closed for %name.', array('%name' => $user->name)));

    // Destroy the current session.
    session_destroy();
    module_invoke_all('user', 'logout', NULL, $user);

    // Load the anonymous user.
    $user = drupal_anonymous_user();

    // Expire the session in our qbwc table and in this class.
    $qbwc = (object) array(
      'qbwc_id' => $this->qbwc_id,
      'sid' => '',
      'ticket' => '',
      'timestamp_end' => time(),
    );
    drupal_write_record('qbwc', $qbwc, 'qbwc_id');
    unset($this->sid);

    return (object) array( 'closeConnectionResult' => t('Session closed'));
  }

  function connectionError($ticket, $hresult, $message) {
    // Do not try to reconnect
    return 'done';
  }

  function getLastError($params) {
    $this->_session($params->ticket);
    $errors = array();
    foreach (qbwc_error() as $error) {
      $errors[] = $error;
    }
    
    return (object) array('getLastErrorResponse' => $errors);
  }

  function serverVersion($ticket) {
    $ret = t('QBWC');
    return $ret;
  }

  function sendRequestXML($params) {
    $this->_session($params->ticket);

    $hcp_response = $params->strHCPResponse;
    $company_file = $params->strCompanyFileName;
    $country = $params->qbXMLCountry;
    $major_version = $params->qbXMLMajorVers;
    $minor_version = $params->qbXMLMinorVers;

    // Default: If the web service has no requests to send, use an empty string.
    $ret = '';

    // Decide if we have anything to do and return qbxml.
    $requests = array();
    foreach (qbwc_request_shift() as $request_id => $r) {
      // Apply filter callbacks to request. Done here because filters usually rely upon previous requested data.
      if (isset($r['filter_callback'])) {
          foreach ($r['filter_callback'] as $function) {
              $function($r); //Pass by reference;
          }
          qbwc_set('filters', true);
      }

      // Attach a request id so we can send responses to the correct module.
      $r['request']['_requestID'] = $request_id;
    
      // Handle the iterator object so we can process reasonable amounts of data
      $iteratorID = qbwc_get('iteratorID');
      if (empty($iteratorID)) {
        $r['request']['_iterator'] = 'Start';    
      } else {
        $r['request']['_iterator'] = 'Continue';
        $r['request']['_iteratorID'] = $iteratorID;
      }

      $r['request'] = array('MaxReturned' => variable_get('qbwc_max_returned', 50)) + $r['request'];

      if ($r['name']) {
        $requests[$r['name']][] = $r['request'];
      }
    }

    if ($requests) {
      // Formulate the common base XML for all QBWC requests
      $version = variable_get('qb_version', QB_VERSION_DEFAULT);
      $top = array(
        'QBXML' => array(
          '?qbxml' => 'version="'. $version .'"',
          'QBXMLMsgsRq' => array(
            '_onError' => 'continueOnError' // XML attribute
          )
        )
      );
  
      $ret = qb_qbxml($requests, $top);
    }
    if (variable_get('qbwc_debug', 0)) watchdog('qbwc_request', print_r($requests, 1), null, WATCHDOG_DEBUG);
    return (object) array( 'sendRequestXMLResult' => $ret );
  }

  function receiveResponseXML($params) {
    try { 
            if (variable_get('qbwc_debug', 0)) watchdog('qbwc_response', print_r($params, 1), null, WATCHDOG_DEBUG);
			$this->_session($params->ticket);

			$response = DOMDocument::loadXML($params->response);
			$hresult = $params->hresult;
			$message = $params->message;

			// Get the original list of requests and reset the queue.
			$requests = qbwc_request_queue();

			// Split the response elements and send them to the appropriate callbacks.
			if (is_object($response)) {

				$qbxml = $response->getElementsByTagName('QBXMLMsgsRs')->item(0);
				foreach ($qbxml->childNodes as $element) {
					if (get_class($element) != 'DOMElement') continue;
					$result_type = $element->nodeName;

					$request_id = $element->getAttribute('requestID');
					$request_status = $element->getAttribute('statusCode');
					$request_message = $element->getAttribute('statusMessage');
					$request_severity = $element->getAttribute('statusSeverity');

					// Get the request that matches this response, then remove it from the queue.
					$request = $requests[$request_id];

					if ($request_status) {
						watchdog('qbwc', '@err: @msg returned for @req request', array('@err' => $request_status, '@msg' => $request_message, '@req' => $request['name']), null, WATCHDOG_ERROR);
					}
					
					$result = array();
					foreach ($element->childNodes as $r) {
						if (get_class($r) == 'DOMElement') $result[] = qb_array($r);
					}

                    // The result set is incomplete: re-submit it to the beginning of the queue.
                    $items_remaining = $element->getAttribute('iteratorRemainingCount');
                    if (!empty($items_remaining) && $items_remaining > 0) {
                        qbwc_set('iteratorID', $element->getAttribute('iteratorID'));
                    } else {
                      qbwc_set('iteratorID', null); // Disable any iterators
                      qbwc_set('filters', null); // Remove any filter set
                      qbwc_request_shift(true); // Remove this request, even if it failed
                    }

					// If there's a callback for this request, call it back.
					if (isset($request['callback'])) {
						foreach ($request['callback'] as $function) {
							$data = isset($request['data']) ? $request['data'] : array();
							if (is_array($function)) {
								$func = $function['function'];
								$options = $function['options'];
								$func($result, $data, $request_status, $request_message, $options);
							}
							else {
								$function($result, $data, $request_status, $request_message);
							}
						}
					} //endif
				} //end foreach
			}

			/** 
			 * A positive integer from 1-100 represents the percentage of work
			 * completed, where value of 100 means there is no more work. A negative
			 * value means an error has occurred and Web Connector calls getLastError.
			 */

			// If there are no new requests in the queue, we're 100% done.
			$done = qbwc_get('requests_done') / qbwc_get('request_id') * 100;

			// Something has logged an error along the way.
			if (qbwc_error()) {
				$done = -1;
			}
			// If it runs this long, something is probably wrong...
			if ($_SESSION['countTimes']++ == 30) {
				$done = 100;
			}

			return (object) array( 'receiveResponseXMLResult' => $done );
		}
		catch (Exception $e) {
			watchdog('qbwc', '@err in @file at line @line', array('@err' => $e->getMessage(), '@file' => $e->getFile(), '@line' => $e->getLine()), null, WATCHDOG_ERROR);
		}
  }

  // Unimplemented functions

  function getInteractiveURL($ticket, $session_id) {
    $this->_session($ticket);
  }

  function interactiveDone($ticket) {
    $this->_session($ticket);
  }

  function interactiveRejected($ticket, $reason) {
    $this->_session($ticket);
  }

  // This function is called prior to authenticate() and is used to specify
  // the client's version.  It could be used to set a minimum version level.
  function clientVersion($version) {
  }

  /**
   * QBWC destructor.  The session id may have been changed by a call to 
   * session_regenerate(), so make sure we can find it next time.
   */
  function __destruct() {
    if ($this->qbwc_id && ($this->sid != session_id())) {
      $qbwc = array('qbwc_id' => $this->qbwc_id, 'sid' => session_id());
      drupal_write_record('qbwc', $qbwc, 'qbwc_id');
    }
  }
}
