CREATE DATABASE IF NOT EXISTS chat
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_0900_ai_ci;

USE chat;

SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS OfflineMessage;
DROP TABLE IF EXISTS GroupUser;
DROP TABLE IF EXISTS Friend;
DROP TABLE IF EXISTS AllGroup;
DROP TABLE IF EXISTS user;

SET FOREIGN_KEY_CHECKS = 1;

CREATE TABLE user (
  id INT NOT NULL AUTO_INCREMENT,
  name VARCHAR(50) NOT NULL,
  password CHAR(64) NOT NULL,
  state ENUM('online', 'offline') NOT NULL DEFAULT 'offline',
  PRIMARY KEY (id),
  UNIQUE KEY uk_user_name (name),
  KEY idx_user_state (state)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE AllGroup (
  id INT NOT NULL AUTO_INCREMENT,
  groupname VARCHAR(50) NOT NULL,
  groupdesc VARCHAR(200) NOT NULL DEFAULT '',
  PRIMARY KEY (id),
  UNIQUE KEY uk_group_name (groupname)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE Friend (
  userid INT NOT NULL,
  friendid INT NOT NULL,
  PRIMARY KEY (userid, friendid),
  KEY idx_friend_friendid (friendid),
  CONSTRAINT fk_friend_user
    FOREIGN KEY (userid) REFERENCES user (id)
    ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT fk_friend_friend
    FOREIGN KEY (friendid) REFERENCES user (id)
    ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE GroupUser (
  groupid INT NOT NULL,
  userid INT NOT NULL,
  grouprole ENUM('creator', 'normal') NOT NULL DEFAULT 'normal',
  PRIMARY KEY (groupid, userid),
  KEY idx_group_userid (userid),
  CONSTRAINT fk_group_user_group
    FOREIGN KEY (groupid) REFERENCES AllGroup (id)
    ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT fk_group_user_user
    FOREIGN KEY (userid) REFERENCES user (id)
    ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE OfflineMessage (
  id BIGINT NOT NULL AUTO_INCREMENT,
  userid INT NOT NULL,
  message VARCHAR(1024) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_offline_userid (userid),
  CONSTRAINT fk_offline_user
    FOREIGN KEY (userid) REFERENCES user (id)
    ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
