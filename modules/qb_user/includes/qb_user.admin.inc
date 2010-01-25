<?php // $Id: qb_user.admin.inc,v 1.1 2009/02/19 05:33:17 vauxia Exp $

function qb_user_admin_form() {
  $form = array();
  $form['#submit'][] = '_qb_user_query_save';

  $roles = user_roles();
  unset($roles[DRUPAL_ANONYMOUS_RID], $roles[DRUPAL_AUTHENTICATED_RID]);

  $form['drupal'] = array(
    '#type' => 'fieldset',
    '#title' => t('Drupal users'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['drupal']['qb_user_enable'] = array(
    '#type' => 'radios',
    '#options' => array(
       'none' => t('Do not map users to Quickbooks'), 
       'all' => t('Map all users to Quickbook contacts'), 
       'role' => t('Only map users in certain roles'),
    ),
    '#default_value' => variable_get('qb_user_enable', 'none'),
  );

  if ($roles) {
    $form['drupal']['qb_user_roles'] = array(
      '#type' => 'checkboxes',
      '#options' => $roles,
      '#default_value' => variable_get('qb_user_roles', array()),
    );
  }
  else {
    $form['drupal']['qb_user_roles'] = array(
      '#type' => 'markup',
      '#value' => '<p>'. t('There are no available roles. Please visit the !link page to add one or more roles.', array('!link' => l(t('roles'), 'admin/user/roles'))) .'</p>',
    );
  }

  $form['quickbooks'] = array(
    '#type' => 'fieldset',
    '#title' => t('Quickbooks contact types'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  foreach (_qb_user_qb_contact_types() as $name => $info) {
    $name = 'qb_user_'. $name;
    $value = variable_get($name, array());
    $form['quickbooks'][$name] = array(
      '#type' => 'fieldset',
      '#title' => $info['title'],
      '#collapsible' => FALSE,
      '#tree' => TRUE,
    );
    if ($value['qdid']) {
      $form['quickbooks'][$name]['qdid'] = array(
        '#type' => 'value', '#value' => $value['qdid'],
      );
    }
    $form['quickbooks'][$name]['request'] = array(
      '#type' => 'value', '#value' => $info['request'],
    );
    $form['quickbooks'][$name]['qb_qry'] = array(
      '#type' => 'checkbox',
      '#title' => t('Import Quickbooks @name as Drupal users', array('@name' => $info['title'])),
      '#default_value' => $value['qb_qry'] ?  $value['qb_qry'] : 0,
    );
    $form['quickbooks'][$name]['qb_add'] = array(
      '#type' => 'checkbox',
      '#title' => t('Export Drupal users as @name', array('@name' => $info['title'])),
      '#default_value' => $value['qb_add'] ?  $value['qb_add'] : 0,
    );
    $form['quickbooks'][$name]['qb_mod'] = array(
      '#type' => 'checkbox',
      '#title' => t('Synchronize changes'),
      '#default_value' => $value['qb_mod'] ?  $value['qb_mod'] : 0,
    );
    $form['quickbooks'][$name]['qb_full_name'] = array(
      '#type' => 'textfield',
      '#title' => t('Full name'),
      '#default_value' => $value['qb_full_name'] ?  $value['qb_full_name'] : '',
      '#description' => t('Quickbooks requires that each contact have a globally unique "Full Name". For example, a Vendor cannot have the same name as a Customer.  If a user is mapped multiple contact types you must ensure that each instane of her Full Name is unique. You might handle this by using a suffix, such as [@name] here', array('@name' => $name)),
    );
    if ($roles) {
      $form['quickbooks'][$name]['roles'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('Drupal roles for @name', array('@name' => $info['title'])),
        '#options' => $roles,
        '#default_value' => $value['roles'] ?  $value['roles'] : array(),
      );
    }
  }

  $form['import'] = array(
    '#type' => 'fieldset',
    '#title' => t('Import behavior'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['import']['qb_user_import_mode'] = array(
    '#type' => 'radios',
    '#title' => t('When new contacts are imported from Quickbooks'),
    '#options' => array(
      'add' => t('Create a new Drupal user'),
      'match' => t('Store the email address, and match it against future users'),
    ),
    '#default_value' => variable_get('qb_user_import_mode', 'match'),
  );
  $form['import']['user'] = array(
    '#type' => 'fieldset',
    '#title' => t('User creation settings'),
  );
  $form['import']['user']['qb_user_name'] = array(
    '#type' => 'textfield',
    '#title' => t('User name'),
    '#default_value' => variable_get('qb_user_name', ''),
  );
  $form['import']['user']['qb_user_notify'] = array(
    '#type' => 'checkbox',
    '#title' => t('Notify user of new account'),
    '#default_value' => variable_get('qb_user_notify', ''),
  );

  if (module_exists('profile')) {
    $form['profile'] = array(
      '#type' => 'fieldset',
      '#title' => t('Profile settings'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    );
  }

  return system_settings_form($form);
}

/**
 * Implementation of a submit hook for the field settings form.
 * We're actually storing the query definition in the QB Data module, so
 * account for it there.
 */
function _qb_user_query_save(&$form, &$form_state) {
  module_load_include('inc', 'qb_data', 'includes/qb_data.admin');
  foreach (_qb_user_qb_contact_types() as $name => $info) {
    $query = NULL;
    $name = 'qb_user_'. $name;
    $edit = $form_state['values'][$name];
    if ($qdid = $edit['qdid']) $query = qb_data_query_load($qdid);
    $edit['title'] = t($info['title']);
    $edit['status'] = $values[''];
    $edit['frequency'] = 0;
    $edit['callbacks'] =  'qb_user_qbwc_map';
    $edit['request'] = $info['request'];
    $query = qb_data_query_save($query, $edit);
    $form_state['values'][$name]['qdid'] = $query->qdid;
  }
}

function _qb_user_qb_contact_types() {
  return array(
    'customer' => array(
      'request' => 'Customer',
      'title' => t('Customers'),
      'description' => t('Names from your customer list.'),
    ),
    'employee' => array(
      'request' => 'Employee',
      'title' => t('Employees'),
      'description' => t('Employees.'),
    ),
    'vendor' => array(
      'request' => 'Vendor',
      'title' => t('Vendors'),
      'description' => t('Names from your vendor list.'),
    ),
    'other' => array(
      'request' => 'OtherName',
      'title' => t('Other contacts'),
      'title' => t('Contacts from your "Other Names" list'),
    ),
  );
}
