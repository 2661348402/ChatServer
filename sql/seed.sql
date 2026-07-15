USE chat;

-- Demo password for all seed users: 123456
-- SHA-256: 8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92

INSERT INTO user (id, name, password, state) VALUES
  (1, 'alice', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', 'offline'),
  (2, 'bob', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', 'offline'),
  (3, 'charlie', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', 'offline')
ON DUPLICATE KEY UPDATE
  name = VALUES(name),
  password = VALUES(password),
  state = 'offline';

INSERT INTO AllGroup (id, groupname, groupdesc) VALUES
  (1, 'demo-room', 'Default demo group')
ON DUPLICATE KEY UPDATE
  groupname = VALUES(groupname),
  groupdesc = VALUES(groupdesc);

INSERT INTO Friend (userid, friendid) VALUES
  (1, 2),
  (2, 1),
  (1, 3),
  (3, 1)
ON DUPLICATE KEY UPDATE
  userid = VALUES(userid);

INSERT INTO GroupUser (groupid, userid, grouprole) VALUES
  (1, 1, 'creator'),
  (1, 2, 'normal'),
  (1, 3, 'normal')
ON DUPLICATE KEY UPDATE
  grouprole = VALUES(grouprole);
