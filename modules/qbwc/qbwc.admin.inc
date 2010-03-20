<?php // $Id: qbwc.admin.inc,v 1.3 2009/08/02 23:52:53 vauxia Exp $

function qbwc_admin_form() {
  // Add QBWC settings to the Quickbooks settings page
  $form = array();
  $form['qbwc_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Web Connector URL'),
    '#description' => t('The URL the Web Connector will use to communicate with your Drupal site.  Please note that it MUST be SSL if the hostname is not "localhost"'),
    '#default_value' => variable_get('qbwc_url', str_replace('http:', 'https:', url('qbwc', array('absolute' => TRUE)))),
  );

  $form['qbwc_user'] = array(
    '#type' => 'textfield',
    '#title' => t('Quickbooks user'),
    '#description' => t('This is the user that will be used to effect Quickbooks transactions.  You can use an existing user, but you probably want to create a user for this purpose'),
    '#autocomplete_path' => 'user/autocomplete',
    '#default_value' => variable_get('qbwc_user', ''),
  );

  $form['qbwc_hostname'] = array(
    '#type'        => 'textfield',
    '#title'       => t('IP Restriction'),
    '#description' => t('If you want to limit connections to the Web Connector service to a limited set of hosts, enter one or more IP addresses here, separated by commas.'),
    '#default_value' => variable_get('qbwc_hostname', '')
  );

  $options = array(
    0  => t('Manually (No scheduling)'),
    5  => t('Every 5 minutes'),
    15 => t('Every 15 minutes'),
    60 => t('Every hour'),
    240 => t('Every 4 hours'),
    480 => t('Every 8 hours'),
    480 => t('Every 8 hours'),
    720 => t('Every 12 hours'),
    1440 => t('Once per day'),
  );
  $form['qbwc_scheduler'] = array(
    '#type'        => 'select',
    '#title'       => 'Run the Web Connector...',
    '#description' => t('Select the time interval you wish to poll this site for updates.'),
    '#default_value' => variable_get('qbwc_scheduler', 0),
    '#options' => $options,
  );

  $form['qbwc_max_returned'] = array(
    '#type'         => 'textfield',
    '#title'        => 'Results Returned Per Request',
    '#descriptiopn' => 'Enter the maximum number of results you would like returned per request. This limit will keep PHP from running out of ram trying to parse the XML returned from Quickbooks and keep the Quickbooks Web Connector from timing out waiting for the Web Service to reply.',
    '#default_value' => variable_get('qbwc_max_returned', 50),
    '#size' => 5,
    '#maxlength' => 5,
  );

  $form['qbwc_debug'] = array(
    '#type'         => 'checkbox',
    '#title'        => 'Enable QBWC Debugging Messages',
    '#description' => 'Enabling this will cause debugging messages to be logged into the watchdog table.',
    '#default_value' => variable_get('qbwc_debug', 0),
  );

  $form['qbwc_qwc'] = array(
    '#type' => 'markup',
    '#value' => l(t('Download QWC file'), 'admin/settings/qb/qwc'),
  );

  return $form;
}

function qbwc_qwc() {
  $elements = array();
  
  // Generate a FileID for this application file
  $part = array();
  foreach(array(8,4,4,4,12) as $len) {
    $part[] = substr(str_pad(base_convert(rand(), 10, 16), $len, "0", STR_PAD_LEFT), 0, $len);
  }

  $elements['AppName'] = variable_get('site_name', 'Drupal');
  $elements['AppID'] = '';
  $elements['AppURL'] = variable_get('qbwc_url', '');
  $elements['AppDescription'] = t('Integration with your Drupal site');
  $elements['AppSupport'] = variable_get('qbwc_url', '') .'/support';
  $elements['UserName'] = variable_get('qbwc_user', '');
  $elements['OwnerID'] = '{' . variable_get('qbwc_owner_id', '') . '}';
  $elements['FileID'] = '{' . implode('-', $part) . '}';
  $elements['QBType'] = 'QBFS'; // Used with QB Financial systems, not POS.
  if ($minutes = variable_get('qbwc_scheduler', 0)) {
    $elements['Scheduler'] = array();
    $elements['Scheduler']['RunEveryNMinutes'] = $minutes;
  }

  $xml = qb_qbxml($elements, 'QBWCXML');

  drupal_set_header('Content-type: application/force-download');
  drupal_set_header('Cache-Control: must-revalidate, post-check=0, pre-check=0');
  drupal_set_header('Content-Transfer-Encoding: Binary');
  drupal_set_header('Content-length: ' . strlen($xml));
  drupal_set_header('Content-disposition: attachment; filename=drupal.qwc');
  echo $xml;
  exit;
}
