<?php // $Id: qb.inc,v 1.5 2009/08/02 23:52:53 vauxia Exp $

/**
 * @file
 * Basic settings page for Quickbooks.
 *
 */

function qb_settings() {
  $form = array();
  $form['qb_version'] = array(
    '#type'        => 'select',
    '#title'       => t('Quickbooks API version'),
    '#options'     => array(
      '5.0' => '5.0',
      '6.0' => '6.0',
      '7.0' => '7.0'
    ),
    '#default_value' => variable_get('qb_version', QB_VERSION_DEFAULT),
  );

  $form['qb_company_file'] = array(
    '#type'        => 'textfield',
    '#title'       => t('Company file'),
    '#description' => t('The full path to your quickbooks company file, e.g. C:\Data\MyCompany\MyCompany.qdb.  If you leave this blank, applications will use the file that is currently open on your workstation.'),
    '#default_value' => variable_get('qb_company_file', ''),
  );
  // Make sure the buttons show up someplace responsible.
  $form['buttons'] = array('#weight' => 10);

  return system_settings_form($form);
}
